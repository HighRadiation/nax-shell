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

/*
** Token turunun bu asamada henuz desteklenmedigini soyler.
**
** Artalan isareti ve parantezler kaldi: ikisi de is denetimi ya da alt
** kabuk gerektiriyor, yani ayristiricinin degil calistiricinin isi.
*/
static int	is_unsupported(t_tok type)
{
	return (type == T_AMP || type == T_LPAREN || type == T_RPAREN);
}

/* Token bir boru hatti baglantisi mi. */
static int	is_join(t_tok type)
{
	return (type == T_SEMI || type == T_AND_IF || type == T_OR_IF);
}

/* Token turunu baglanti turune cevirir. */
static t_join	join_of(t_tok type)
{
	if (type == T_SEMI)
		return (J_SEMI);
	if (type == T_AND_IF)
		return (J_AND);
	if (type == T_OR_IF)
		return (J_OR);
	return (J_FIRST);
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
	if (par->list_tail != NULL)
		par->list_tail->cmds = par->head;
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

/*
** Sozcugun tirnakli bir parcasi var mi.
**
** "<<" sinirlayicisi icin gerekli: tirnakliysa govde genisletilmez.
** Kural sinirlayicinin YAZIMINA bakiyor, govdenin icerigine degil; bash
** de boyle davraniyor.
*/
static int	quoted_word(const t_token *word)
{
	const t_seg	*seg;

	if (word == NULL)
		return (0);
	seg = word->segs;
	while (seg != NULL)
	{
		if (seg->quote != Q_NONE)
			return (1);
		seg = seg->next;
	}
	return (0);
}

/* Yonlendirmeyi komuta ekler; hedef sozcuk odunc alinir. */static int	redir_push(t_parser *par, t_cmd *cmd, t_tok type,
		const t_token *target)
{
	t_redir	*redir;

	redir = malloc(sizeof(*redir));
	if (redir == NULL)
		return (0);
	redir->type = type;
	redir->target = target;
	redir->body = NULL;
	redir->raw = quoted_word(target);
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

/*
** Bos komut hatasini baglama gore bildirir.
**
** NEDEN IKI AYRI MESAJ: "ls |" ile "ls &&" ayni sinifta degil. Kullanici
** hangi isaretin iki yanina komut bekledigini bilmek istiyor ve tek
** mesaj ikisini de bulanik hale getirirdi.
**
** MESAJ KARSILASILAN TOKEN'A GORE SECILIR, onceki operatore gore degil.
** "a && | b" satirinda suclu olan boru isareti; bash de beklenmeyen
** token'i adlandiriyor ("syntax error near unexpected token |").
** Onceki operatore bakmak bu satirda yanlis isareti gosterirdi.
*/
static int	empty_cmd_error(t_parser *par)
{
	if (par->tok != NULL && par->tok->type == T_PIPE)
		return (fail(par, "boru isaretinin iki yaninda da komut olmali",
				par->tok));
	if (par->tok != NULL && is_join(par->tok->type))
		return (fail(par, "operatorun iki yaninda da komut olmali",
				par->tok));
	if (par->list_tail != NULL && par->list_tail->join != J_FIRST)
		return (fail(par, "operatorun iki yaninda da komut olmali",
				par->tok));
	return (fail(par, "boru isaretinin iki yaninda da komut olmali",
			par->tok));
}

/* Tek bir komutu boru isaretine, baglantiya ya da satir sonuna kadar okur. */
static int	read_cmd(t_parser *par)
{
	t_cmd	*cmd;

	cmd = cmd_push(par);
	if (cmd == NULL)
		return (0);
	while (par->tok != NULL && par->tok->type != T_PIPE
		&& is_join(par->tok->type) == 0)
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
		return (empty_cmd_error(par));
	return (1);
}

/*
** Listeye yeni bir boru hatti ekler ve komut imlecini sifirlar.
**
** Hat listeye HEMEN baglaniyor; yarim kalmis bir hattin listede olmamasi
** hata yolunda cift serbest birakma riski uretirdi.
*/
static int	pipeline_push(t_parser *par, t_join join)
{
	t_pipeline	*node;

	node = malloc(sizeof(*node));
	if (node == NULL)
		return (0);
	node->cmds = NULL;
	node->join = join;
	node->next = NULL;
	if (par->list_tail == NULL)
		par->list_head = node;
	else
		par->list_tail->next = node;
	par->list_tail = node;
	par->head = NULL;
	par->tail = NULL;
	return (1);
}

/* Bir boru hattini sonuna kadar okur. */
static int	read_pipeline(t_parser *par)
{
	while (1)
	{
		if (read_cmd(par) == 0)
			return (0);
		if (par->tok == NULL || is_join(par->tok->type))
			return (1);
		par->tok = par->tok->next;
		if (par->tok == NULL || is_join(par->tok->type))
			return (fail(par, "boru isaretinin iki yaninda da komut olmali",
					par->tok));
	}
}

/* Ayristirici durumunu ilk degerlerine kurar. */
static void	parser_init(t_parser *par, const t_token *tokens, t_ast_err *err)
{
	par->tok = tokens;
	par->head = NULL;
	par->tail = NULL;
	par->arg_tail = NULL;
	par->redir_tail = NULL;
	par->list_head = NULL;
	par->list_tail = NULL;
	par->err = err;
	err->message = NULL;
	err->at = NULL;
}

/* Hata yolunda yarim kalmis agaci birakir ve NULL doner. */
static t_pipeline	*parser_abort(t_parser *par)
{
	ast_free(par->list_head);
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
		free(redir->body);
		free(redir);
		redir = next;
	}
}

/* Bir boru hattinin komutlarini serbest birakir. */
static void	cmds_free(t_cmd *cmds)
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

/* Boru hatti listesini serbest birakir; token'lara dokunmaz. */
void	ast_free(t_pipeline *list)
{
	t_pipeline	*next;

	while (list != NULL)
	{
		next = list->next;
		cmds_free(list->cmds);
		free(list);
		list = next;
	}
}

/*
** Token listesini boru hatti listesine cevirir.
**
** Sonuc bir LISTE: ";", "&&" ve "||" ile ayrilmis boru hatlari. Tek
** hatlik girdide liste tek elemanli olur, yani bu degisiklik eski
** davranisi hic bozmuyor.
**
** AGAC TOKEN'LARI ODUNC ALIR: sozcukler kopyalanmaz, token listesine
** isaret edilir. Cagiran ikisini de serbest birakir ve bu, hata
** yolunda kismi sahiplik sorunu olmamasini sagliyor.
*/
t_pipeline	*ast_build(const t_token *tokens, t_ast_err *err)
{
	t_parser	par;
	t_join		join;

	parser_init(&par, tokens, err);
	if (par.tok == NULL)
		return (NULL);
	join = J_FIRST;
	while (1)
	{
		if (pipeline_push(&par, join) == 0)
			return (parser_abort(&par));
		if (read_pipeline(&par) == 0)
			return (parser_abort(&par));
		if (par.tok == NULL)
			break ;
		join = join_of(par.tok->type);
		par.tok = par.tok->next;
		if (par.tok == NULL)
		{
			fail(&par, "operatorun iki yaninda da komut olmali", NULL);
			return (parser_abort(&par));
		}
	}
	return (par.list_head);
}
