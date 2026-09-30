/*
** lex_dump.c — token listesini okunabilir kanonik metne cevirir.
**
** NEDEN AYRI MODUL:
**   Bu, sozcuk ayirmadan farkli bir is: ayirma girdiyi yapiya cevirir,
**   burasi yapiyi metne cevirir. Ayni dosyada tutmak lexer.c'yi bes yuz
**   satirin uzerine cikariyordu.
**
**   Uretilen metin iki yerde kullanilir: tablo tabanli testlerin
**   karsilastirma bicimi, ve elle hata ayiklarken en hizli bakis.
**
** BICIM:
**   sozcuk    W(kip:metin+kip:metin)
**   operator  literal isareti
**   kipler    none | dq | sq
**
** ORNEK:
**   echo "a b" | wc -l
**   W(none:echo) W(dq:a b) | W(none:wc) W(none:-l)
*/

#include "nax.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>

/* Parca kipinin kisa adini dondurur. */
static const char	*quote_name(t_quote quote)
{
	if (quote == Q_SINGLE)
		return ("sq");
	if (quote == Q_DOUBLE)
		return ("dq");
	return ("none");
}

/* Token turunun kanonik isaretini dondurur. */
const char	*lex_name(t_tok type)
{
	if (type == T_WORD)
		return ("W");
	if (type == T_PIPE)
		return ("|");
	if (type == T_REDIR_IN)
		return ("<");
	if (type == T_REDIR_OUT)
		return (">");
	if (type == T_APPEND)
		return (">>");
	if (type == T_HEREDOC)
		return ("<<");
	if (type == T_SEMI)
		return (";");
	if (type == T_AND_IF)
		return ("&&");
	if (type == T_OR_IF)
		return ("||");
	if (type == T_AMP)
		return ("&");
	if (type == T_LPAREN)
		return ("(");
	return (")");
}

/* Bir sozcugun parcalarini tampona yazar. */
static int	dump_word(t_buf *buf, const t_seg *segs)
{
	if (buf_push_str(buf, "W(") == 0)
		return (0);
	while (segs != NULL)
	{
		if (buf_push_str(buf, quote_name(segs->quote)) == 0)
			return (0);
		if (buf_push(buf, ':') == 0)
			return (0);
		if (buf_push_str(buf, segs->text) == 0)
			return (0);
		segs = segs->next;
		if (segs != NULL && buf_push(buf, '+') == 0)
			return (0);
	}
	return (buf_push(buf, ')'));
}

/* Token listesini kanonik metne cevirir; cagiran serbest birakir. */
char	*lex_dump(const t_token *tokens)
{
	t_buf	buf;

	buf_init(&buf);
	while (tokens != NULL)
	{
		if (tokens->type == T_WORD)
		{
			if (dump_word(&buf, tokens->segs) == 0)
				return (NULL);
		}
		else if (buf_push_str(&buf, lex_name(tokens->type)) == 0)
			return (NULL);
		tokens = tokens->next;
		if (tokens != NULL && buf_push(&buf, ' ') == 0)
			return (NULL);
	}
	if (buf.data == NULL)
		return (strdup(""));
	return (buf.data);
}
