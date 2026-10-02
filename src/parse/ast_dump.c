/*
** ast_dump.c - boru hatti agacini okunabilir kanonik metne cevirir.
**
** NEDEN AYRI MODUL:
**   Sozcuk ayirmadaki ayrimin aynisi: parser.c girdiyi yapiya cevirir,
**   burasi yapiyi metne. Uretilen metin iki yerde kullanilir: tablo
**   tabanli testlerin karsilastirma bicimi, ve elle hata ayiklarken
**   agacin seklini gormenin en hizli yolu.
**
** BICIM:
**   sozcuk       [metin]
**   yonlendirme  <[dosya]  >[dosya]  >>[dosya]  <<[sinirlayici]
**   komut        (cmd [w1] [w2] >[dosya])
**   boru hatti   (pipe (cmd ...) (cmd ...))
**   liste        (list (cmd ...) && (cmd ...))
**
**   Tek komutluk boru hattinda (pipe ...), tek hatlik listede (list ...)
**   sarmalayicisi yazilmaz. Boylece yeni katmanlar eklendiginde eski
**   vakalarin dokumu degismiyor.
**
** NEDEN SOZCUKLER KOSELI PARANTEZ ICINDE:
**   Bir sozcuk bosluk icerebilir: cat "a b" iki degil BIR argumandir.
**   Parantez olmasa ikisi ayirt edilemezdi. Tirnak kipi burada
**   yazilmiyor, cunku ayristirici tirnaga bakmaz; tirnak ayrintisi
**   sozcuk ayiricinin test bicimine ait.
*/

#include "nax.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>

/* Sozcugun tum parcalarini birlestirip koseli parantez icinde yazar. */
static int	dump_word(t_buf *buf, const t_token *word)
{
	const t_seg	*seg;

	if (buf_push(buf, '[') == 0)
		return (0);
	seg = word->segs;
	while (seg != NULL)
	{
		if (buf_push_str(buf, seg->text) == 0)
			return (0);
		seg = seg->next;
	}
	return (buf_push(buf, ']'));
}

/* Komutun argumanlarini yazildiklari sirada yazar. */
static int	dump_args(t_buf *buf, const t_arg *arg)
{
	while (arg != NULL)
	{
		if (buf_push(buf, ' ') == 0)
			return (0);
		if (dump_word(buf, arg->word) == 0)
			return (0);
		arg = arg->next;
	}
	return (1);
}

/* Komutun yonlendirmelerini yazildiklari sirada yazar. */
static int	dump_redirs(t_buf *buf, const t_redir *redir)
{
	while (redir != NULL)
	{
		if (buf_push(buf, ' ') == 0)
			return (0);
		if (buf_push_str(buf, lex_name(redir->type)) == 0)
			return (0);
		if (dump_word(buf, redir->target) == 0)
			return (0);
		redir = redir->next;
	}
	return (1);
}

/* Tek bir komutu yazar. */
static int	dump_cmd(t_buf *buf, const t_cmd *cmd)
{
	if (buf_push_str(buf, "(cmd") == 0)
		return (0);
	if (dump_args(buf, cmd->args) == 0)
		return (0);
	if (dump_redirs(buf, cmd->redirs) == 0)
		return (0);
	return (buf_push(buf, ')'));
}

/* Boru hattini yazar; tek komutluysa sarmalayici kullanmaz. */
static int	dump_pipeline(t_buf *buf, const t_cmd *cmds)
{
	int	many;

	many = (cmds != NULL && cmds->next != NULL);
	if (many && buf_push_str(buf, "(pipe ") == 0)
		return (0);
	while (cmds != NULL)
	{
		if (dump_cmd(buf, cmds) == 0)
			return (0);
		cmds = cmds->next;
		if (cmds != NULL && buf_push(buf, ' ') == 0)
			return (0);
	}
	if (many && buf_push(buf, ')') == 0)
		return (0);
	return (1);
}

static const char	*join_text(t_join join)
{
	if (join == J_SEMI)
		return (" ; ");
	if (join == J_AND)
		return (" && ");
	if (join == J_OR)
		return (" || ");
	return ("");
}

/*
** Boru hatti listesini yazar.
**
** TEK HATLIK LISTEDE "(list ...)" SARMALAYICISI YAZILMAZ. Boylece
** operator icermeyen girdilerin dokumu, bu katman eklenmeden onceki
** haliyle birebir ayni kaliyor ve mevcut vakalarin hicbiri degismiyor.
*/
static int	dump_list(t_buf *buf, const t_pipeline *list)
{
	int	many;

	many = (list != NULL && list->next != NULL);
	if (many && buf_push_str(buf, "(list ") == 0)
		return (0);
	while (list != NULL)
	{
		if (buf_push_str(buf, join_text(list->join)) == 0)
			return (0);
		if (dump_pipeline(buf, list->cmds) == 0)
			return (0);
		list = list->next;
	}
	if (many && buf_push(buf, ')') == 0)
		return (0);
	return (1);
}

/* Boru hatti listesini kanonik metne cevirir; cagiran serbest birakir. */
char	*ast_dump(const t_pipeline *list)
{
	t_buf	buf;

	buf_init(&buf);
	if (dump_list(&buf, list) == 0)
	{
		buf_free(&buf);
		return (NULL);
	}
	if (buf.data == NULL)
		return (strdup(""));
	return (buf.data);
}
