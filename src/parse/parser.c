/*
** parser.c - token listesini boru hatti agacina cevirir.
**
** BU ASAMADAKI DILBILGISI:
**   boru_hatti  : komut ( '|' komut )*
**   komut       : ( sozcuk | yonlendirme )+
**   yonlendirme : ( '<' | '>' | '>>' | '<<' ) sozcuk
**
**   Sozcuk ayirici ';' '&&' '||' '&' '(' ')' operatorlerini de taniyor,
**   ama bu asamada ayristirilmiyorlar; anlasilir bir hata verilir. Onlari
**   simdi tanimak sozcuk ayiriciyi ikinci kez acmayi onluyor.
**
** SAHIPLIK KARARI - agac token'lari ODUNC ALIR:
**   Agac dugumleri yalnizca kendi struct'larinin sahibidir; icerdikleri
**   sozcuk token'lari sozcuk ayiricinin listesinde kalir.
**
**   Neden boyle: alternatifi agacin token listesini devralmasiydi. O
**   durumda ayristirma yarida hata verdiginde token'larin bir kismi
**   agacta, bir kismi listede kalirdi ve kimin neyi birakacagi her hata
**   yolunda yeniden dusunulmek zorunda kalirdi. Odunc alma bu soruyu
**   tamamen ortadan kaldiriyor: cagiran her zaman ikisini de birakir,
**   sirasi da onemli degil.
**
**   Bedeli: token listesi agactan once birakilamaz. Bu kural
**   docs/ARCHITECTURE.md bellek sahipligi tablosunda yazili.
**
** BOS KOMUTA IZIN VAR MI:
**   Arguman icermeyen ama yonlendirmesi olan komut GECERLIDIR; "> dosya"
**   dosyayi olusturup hicbir sey calistirmamak demektir. Ikisi de bos olan
**   komut ise sozdizimi hatasidir: "ls | | wc" boyle yakalanir.
*/

#include "parse.h"
#include <stdlib.h>

/* Token turunun bir yonlendirme operatoru olup olmadigini soyler. */
static int	is_redir(t_tok type)
{
	return (type == T_REDIR_IN || type == T_REDIR_OUT
		|| type == T_APPEND || type == T_HEREDOC);
}

/* Token turunun bu asamada henuz desteklenmedigini soyler. */
static int	is_unsupported(t_tok type)
{
	return (type == T_SEMI || type == T_AND_IF || type == T_OR_IF
		|| type == T_AMP || type == T_LPAREN || type == T_RPAREN);
}

/* Hatayi kaydeder ve her zaman 0 doner; cagiran dogrudan dondurebilir. */
static int	fail(t_parser *par, const char *message, const t_token *at)
{
	par->err->message = message;
	par->err->at = at;
	return (0);
}

/* Boru hattinin sonuna bos bir komut ekler ve onu dondurur. */
static t_cmd	*cmd_push(t_parser *par)
{
	t_cmd	*cmd;

	cmd = malloc(sizeof(*cmd));
	if (cmd == NULL)
		return (NULL);
	cmd->args = NULL;
	cmd->redirs = NULL;
	cmd->next = NULL;
	if (par->tail == NULL)
		par->head = cmd;
	else
		par->tail->next = cmd;
	par->tail = cmd;
	par->arg_tail = NULL;
	par->redir_tail = NULL;
	return (cmd);
}

/* Sozcugu komutun arguman listesinin sonuna ekler. */
static int	arg_push(t_parser *par, t_cmd *cmd, const t_token *word)
{
	t_arg	*arg;

	arg = malloc(sizeof(*arg));
	if (arg == NULL)
		return (0);
	arg->word = word;
	arg->next = NULL;
	if (par->arg_tail == NULL)
		cmd->args = arg;
	else
		par->arg_tail->next = arg;
	par->arg_tail = arg;
	return (1);
}

/* Yonlendirmeyi komutun yonlendirme listesinin sonuna ekler. */
static int	redir_push(t_parser *par, t_cmd *cmd, t_tok type,
		const t_token *target)
{
	t_redir	*redir;

	redir = malloc(sizeof(*redir));
	if (redir == NULL)
		return (0);
	redir->type = type;
	redir->target = target;
	redir->next = NULL;
	if (par->redir_tail == NULL)
		cmd->redirs = redir;
	else
		par->redir_tail->next = redir;
	par->redir_tail = redir;
	return (1);
}

/* Yonlendirme operatoru ve hedefini okur; hedef yoksa hata verir. */
static int	read_redir(t_parser *par, t_cmd *cmd)
{
	t_tok			type;
	const t_token	*target;

	type = par->tok->type;
	par->tok = par->tok->next;
	if (par->tok == NULL || par->tok->type != T_WORD)
	{
		return (fail(par, "yonlendirmeden sonra dosya adi bekleniyor",
				par->tok));
	}
	target = par->tok;
	par->tok = par->tok->next;
	return (redir_push(par, cmd, type, target));
}

/* Bir sozcugu argumana cevirir ve imleci ilerletir. */
static int	read_arg(t_parser *par, t_cmd *cmd)
{
	const t_token	*word;

	word = par->tok;
	par->tok = par->tok->next;
	return (arg_push(par, cmd, word));
}

/* Tek bir komutu boru isaretine ya da satir sonuna kadar okur. */
static int	read_cmd(t_parser *par)
{
	t_cmd	*cmd;

	cmd = cmd_push(par);
	if (cmd == NULL)
		return (0);
	while (par->tok != NULL && par->tok->type != T_PIPE)
	{
		if (is_unsupported(par->tok->type))
			return (fail(par, "bu asamada desteklenmeyen operator", par->tok));
		if (is_redir(par->tok->type))
		{
			if (read_redir(par, cmd) == 0)
				return (0);
		}
		else if (read_arg(par, cmd) == 0)
			return (0);
	}
	if (cmd->args == NULL && cmd->redirs == NULL)
		return (fail(par, "boru isaretinin iki yaninda da komut olmali",
				par->tok));
	return (1);
}

/* Ayristirici durumunu ilk degerlerine kurar. */
static void	parser_init(t_parser *par, const t_token *tokens, t_ast_err *err)
{
	par->tok = tokens;
	par->head = NULL;
	par->tail = NULL;
	par->arg_tail = NULL;
	par->redir_tail = NULL;
	par->err = err;
	err->message = NULL;
	err->at = NULL;
}

/* Hata yolunda yarim kalmis agaci birakir ve NULL doner. */
static t_cmd	*parser_abort(t_parser *par)
{
	ast_free(par->head);
	if (par->err->message == NULL)
		par->err->message = "bellek ayrilamadi";
	return (NULL);
}

/* Arguman listesini serbest birakir; token'lara dokunmaz. */
static void	arg_free(t_arg *arg)
{
	t_arg	*next;

	while (arg != NULL)
	{
		next = arg->next;
		free(arg);
		arg = next;
	}
}

/* Yonlendirme listesini serbest birakir; token'lara dokunmaz. */
static void	redir_free(t_redir *redir)
{
	t_redir	*next;

	while (redir != NULL)
	{
		next = redir->next;
		free(redir);
		redir = next;
	}
}

/* Boru hattini serbest birakir; sozcuk token'lari cagirana kalir. */
void	ast_free(t_cmd *cmds)
{
	t_cmd	*next;

	while (cmds != NULL)
	{
		next = cmds->next;
		arg_free(cmds->args);
		redir_free(cmds->redirs);
		free(cmds);
		cmds = next;
	}
}

/* Token listesini boru hattina cevirir; hata halinde NULL doner. */
t_cmd	*ast_build(const t_token *tokens, t_ast_err *err)
{
	t_parser	par;

	parser_init(&par, tokens, err);
	if (par.tok == NULL)
		return (NULL);
	if (par.tok->type == T_PIPE)
	{
		fail(&par, "boru isaretinin iki yaninda da komut olmali", par.tok);
		return (parser_abort(&par));
	}
	while (1)
	{
		if (read_cmd(&par) == 0)
			return (parser_abort(&par));
		if (par.tok == NULL)
			break ;
		par.tok = par.tok->next;
		if (par.tok == NULL)
		{
			fail(&par, "boru isaretinin iki yaninda da komut olmali", NULL);
			return (parser_abort(&par));
		}
	}
	return (par.head);
}
