/*
** exp_dump.c - genisletilmis boru hattini kanonik metne cevirir.
**
** NEDEN VAR:
**   ast_dump agacin SEKLINI gosteriyor, burasi son DEGERLERI. Ikisi ayri
**   olmak zorunda, cunku ikisi ayri seyi dogruluyor: agac dogru mu
**   kuruldu, ve genisletme dogru mu calisti.
**
** BICIM:
**   ast_dump ile ayni - (cmd [alan] [alan] >[dosya]) ve (pipe ...).
**   Ayni bicim, farkli icerik: koseli parantezler artik sozcuk degil,
**   genisletme sonucu olusan ALANLARI tutuyor.
**
**   Boylece test tablolarinda fark gorunur oluyor:
**     echo $X   agac  -> (cmd [echo] [$X])
**               deger -> (cmd [echo] [a] [b])
*/

#include "nax.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>

/* Bir alani koseli parantez icinde yazar. */
static int	dump_field(t_buf *buf, const char *text)
{
	if (buf_push(buf, '[') == 0)
		return (0);
	if (buf_push_str(buf, text) == 0)
		return (0);
	return (buf_push(buf, ']'));
}

/* Genisletilmis argumanlari sirasiyla yazar. */
static int	dump_args(t_buf *buf, const t_field *field)
{
	while (field != NULL)
	{
		if (buf_push(buf, ' ') == 0)
			return (0);
		if (dump_field(buf, field->text) == 0)
			return (0);
		field = field->next;
	}
	return (1);
}

/* Cozulmus yonlendirmeleri sirasiyla yazar. */
static int	dump_redirs(t_buf *buf, const t_xredir *redir)
{
	while (redir != NULL)
	{
		if (buf_push(buf, ' ') == 0)
			return (0);
		if (buf_push_str(buf, lex_name(redir->type)) == 0)
			return (0);
		if (dump_field(buf, redir->path) == 0)
			return (0);
		redir = redir->next;
	}
	return (1);
}

/* Tek bir komutu genisletip yazar. */
static int	dump_cmd(t_buf *buf, const t_cmd *cmd, const t_shell *sh,
		t_exp_err *err)
{
	t_xcmd	xcmd;
	int		ok;

	if (exp_cmd(cmd, sh, &xcmd, err) == 0)
		return (0);
	ok = buf_push_str(buf, "(cmd");
	if (ok)
		ok = dump_args(buf, xcmd.args);
	if (ok)
		ok = dump_redirs(buf, xcmd.redirs);
	if (ok)
		ok = buf_push(buf, ')');
	xcmd_free(&xcmd);
	return (ok);
}

/* Boru hattini yazar; tek komutluysa sarmalayici kullanmaz. */
static int	dump_pipeline(t_buf *buf, const t_cmd *cmds, const t_shell *sh,
		t_exp_err *err)
{
	int	many;

	many = (cmds != NULL && cmds->next != NULL);
	if (many && buf_push_str(buf, "(pipe ") == 0)
		return (0);
	while (cmds != NULL)
	{
		if (dump_cmd(buf, cmds, sh, err) == 0)
			return (0);
		cmds = cmds->next;
		if (cmds != NULL && buf_push(buf, ' ') == 0)
			return (0);
	}
	if (many && buf_push(buf, ')') == 0)
		return (0);
	return (1);
}

/*
** Genisletilmis boru hattini kanonik metne cevirir.
**
** Hata halinde NULL doner ve err dolar; cagiran serbest birakir.
*/
char	*exp_dump(const t_cmd *cmds, const t_shell *sh, t_exp_err *err)
{
	t_buf	buf;

	buf_init(&buf);
	err->message = NULL;
	err->at = NULL;
	if (dump_pipeline(&buf, cmds, sh, err) == 0)
	{
		buf_free(&buf);
		return (NULL);
	}
	if (buf.data == NULL)
		return (strdup(""));
	return (buf.data);
}
