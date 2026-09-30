/*
** lexer.c — girdi satirini token listesine cevirir.
**
** NEDEN VAR:
**   Kabuk hattinin ilk asamasi. Buradan sonraki her sey token listesi
**   uzerinde calisir; ham metne bir daha donulmez.
**
** NE YAPMAZ:
**   Tirnaklari SILMEZ, degiskenleri GENISLETMEZ, joker karakter
**   COZMEZ. Yalnizca tirnaklarin nerede basladigini isaretler ve bu
**   bilgiyi parca kipi olarak kaydeder. Tirnak kaldirma ve genisletme
**   sonraki asamanin isi; gerekcesi parse.h icinde yazili.
**
** KACIS KARAKTERI:
**   Ters egik cizgi bir karakteri kendi parcasina ayirir ve Q_SINGLE
**   yapar. Bu, kacis karakterinin anlamini tek bir yerde tutar: "bu
**   karakter harfi harfine alinacak". Genisletme asamasinda ayrica kacis
**   kontrolu yapilmasi gerekmez.
*/

#include "nax.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>

/* Karakterin sozcukleri ayiran bir bosluk olup olmadigini soyler. */
static int	is_blank(char c)
{
	return (c == ' ' || c == '\t');
}

/* Karakterin bir operatorun ilk harfi olup olmadigini soyler. */
static int	is_op_start(char c)
{
	return (c == '|' || c == '<' || c == '>' || c == ';'
		|| c == '&' || c == '(' || c == ')');
}

/* Parca listesini serbest birakir. */
static void	seg_free(t_seg *seg)
{
	t_seg	*next;

	while (seg != NULL)
	{
		next = seg->next;
		free(seg->text);
		free(seg);
		seg = next;
	}
}

/* Verilen metni yeni bir parca olarak listenin sonuna ekler. */
static int	seg_push(t_lexer *lex, char *text, t_quote quote)
{
	t_seg	*seg;

	seg = malloc(sizeof(*seg));
	if (seg == NULL)
	{
		free(text);
		return (0);
	}
	seg->text = text;
	seg->quote = quote;
	seg->next = NULL;
	if (lex->seg_tail == NULL)
		lex->seg_head = seg;
	else
		lex->seg_tail->next = seg;
	lex->seg_tail = seg;
	return (1);
}

/* Tamponda birikmis metin varsa onu verilen kipte bir parcaya cevirir. */
static int	buf_flush(t_lexer *lex, t_quote quote)
{
	if (buf_empty(&lex->buf))
		return (1);
	return (seg_push(lex, buf_take(&lex->buf), quote));
}

/* Hatayi kaydeder ve her zaman 0 doner; cagiran dogrudan dondurebilir. */
static int	fail(t_lexer *lex, const char *message, size_t at)
{
	lex->err->message = message;
	lex->err->at = at;
	return (0);
}

/* Tek tirnakli parcayi okur; icerik harfi harfine alinir. */
static int	read_single(t_lexer *lex)
{
	size_t	open_at;

	open_at = lex->i;
	if (buf_flush(lex, Q_NONE) == 0)
		return (0);
	lex->i++;
	while (lex->src[lex->i] != '\'')
	{
		if (lex->src[lex->i] == '\0')
			return (fail(lex, "kapanmamis tek tirnak", open_at));
		if (buf_push(&lex->buf, lex->src[lex->i]) == 0)
			return (0);
		lex->i++;
	}
	lex->i++;
	if (lex->buf.data == NULL)
		return (seg_push(lex, strdup(""), Q_SINGLE));
	return (seg_push(lex, buf_take(&lex->buf), Q_SINGLE));
}

/*
** Cift tirnak icindeki kacis karakterini isler.
**
** Cift tirnak icinde ters egik cizgi yalnizca $ " \ karakterlerini
** kacirir; digerlerinin onunde harfi harfine kalir. Kacirilan karakter
** kendi parcasina ayrilip Q_SINGLE yapilir, boylece genisletmeye
** ugramaz.
*/
static int	read_double_escape(t_lexer *lex)
{
	char	next;

	next = lex->src[lex->i + 1];
	if (next == '$' || next == '"' || next == '\\')
	{
		if (buf_flush(lex, Q_DOUBLE) == 0)
			return (0);
		lex->i += 2;
		if (buf_push(&lex->buf, next) == 0)
			return (0);
		return (seg_push(lex, buf_take(&lex->buf), Q_SINGLE));
	}
	if (buf_push(&lex->buf, '\\') == 0)
		return (0);
	lex->i++;
	return (1);
}

/* Cift tirnakli parcayi okur; genisletmeye acik ama alan ayirmaya kapali. */
static int	read_double(t_lexer *lex)
{
	size_t	open_at;

	open_at = lex->i;
	if (buf_flush(lex, Q_NONE) == 0)
		return (0);
	lex->i++;
	while (lex->src[lex->i] != '"')
	{
		if (lex->src[lex->i] == '\0')
			return (fail(lex, "kapanmamis cift tirnak", open_at));
		if (lex->src[lex->i] == '\\' && lex->src[lex->i + 1] != '\0')
		{
			if (read_double_escape(lex) == 0)
				return (0);
			continue ;
		}
		if (buf_push(&lex->buf, lex->src[lex->i]) == 0)
			return (0);
		lex->i++;
	}
	lex->i++;
	if (lex->buf.data == NULL)
		return (seg_push(lex, strdup(""), Q_DOUBLE));
	return (seg_push(lex, buf_take(&lex->buf), Q_DOUBLE));
}

/* Tirnak disindaki kacis karakterini isler. */
static int	read_escape(t_lexer *lex)
{
	if (lex->src[lex->i + 1] == '\0')
		return (fail(lex, "satir sonunda ters egik cizgi", lex->i));
	if (buf_flush(lex, Q_NONE) == 0)
		return (0);
	lex->i++;
	if (buf_push(&lex->buf, lex->src[lex->i]) == 0)
		return (0);
	lex->i++;
	return (seg_push(lex, buf_take(&lex->buf), Q_SINGLE));
}

/* Token listesinin sonuna verilen turde bir token ekler. */
static int	tok_push(t_lexer *lex, t_tok type, t_seg *segs)
{
	t_token	*tok;

	tok = malloc(sizeof(*tok));
	if (tok == NULL)
	{
		seg_free(segs);
		return (0);
	}
	tok->type = type;
	tok->segs = segs;
	tok->next = NULL;
	if (lex->tail == NULL)
		lex->head = tok;
	else
		lex->tail->next = tok;
	lex->tail = tok;
	return (1);
}

/* Bir sozcugu sonuna kadar okur ve token olarak ekler. */
static int	read_word(t_lexer *lex)
{
	char	c;

	lex->seg_head = NULL;
	lex->seg_tail = NULL;
	while (1)
	{
		c = lex->src[lex->i];
		if (c == '\0' || is_blank(c) || is_op_start(c))
			break ;
		if (c == '\'' && read_single(lex) == 0)
			return (0);
		else if (c == '"' && read_double(lex) == 0)
			return (0);
		else if (c == '\\' && read_escape(lex) == 0)
			return (0);
		else if (c != '\'' && c != '"' && c != '\\')
		{
			if (buf_push(&lex->buf, c) == 0)
				return (0);
			lex->i++;
		}
	}
	if (buf_flush(lex, Q_NONE) == 0)
		return (0);
	return (tok_push(lex, T_WORD, lex->seg_head));
}

/* Iki karakterli operatorleri tanir; yoksa T_WORD doner. */
static t_tok	two_char_op(const char *s)
{
	if (s[0] == '>' && s[1] == '>')
		return (T_APPEND);
	if (s[0] == '<' && s[1] == '<')
		return (T_HEREDOC);
	if (s[0] == '&' && s[1] == '&')
		return (T_AND_IF);
	if (s[0] == '|' && s[1] == '|')
		return (T_OR_IF);
	return (T_WORD);
}

/* Tek karakterli operatorleri tanir. */
static t_tok	one_char_op(char c)
{
	if (c == '|')
		return (T_PIPE);
	if (c == '<')
		return (T_REDIR_IN);
	if (c == '>')
		return (T_REDIR_OUT);
	if (c == ';')
		return (T_SEMI);
	if (c == '&')
		return (T_AMP);
	if (c == '(')
		return (T_LPAREN);
	return (T_RPAREN);
}

/* Bir operatoru okur ve token olarak ekler; uzun bicim once denenir. */
static int	read_op(t_lexer *lex)
{
	t_tok	type;

	type = two_char_op(lex->src + lex->i);
	if (type != T_WORD)
	{
		lex->i += 2;
		return (tok_push(lex, type, NULL));
	}
	type = one_char_op(lex->src[lex->i]);
	lex->i++;
	return (tok_push(lex, type, NULL));
}

/* Lexer durumunu ilk degerlerine kurar. */
static void	lex_init(t_lexer *lex, const char *line, t_lex_err *err)
{
	lex->src = line;
	lex->i = 0;
	lex->head = NULL;
	lex->tail = NULL;
	lex->seg_head = NULL;
	lex->seg_tail = NULL;
	buf_init(&lex->buf);
	lex->err = err;
	err->message = NULL;
	err->at = 0;
}

/* Hata yolunda yarim kalmis her seyi birakir ve NULL doner. */
static t_token	*lex_abort(t_lexer *lex)
{
	lex_free(lex->head);
	seg_free(lex->seg_head);
	buf_free(&lex->buf);
	if (lex->err->message == NULL)
		lex->err->message = "bellek ayrilamadi";
	return (NULL);
}

/* Token listesini ve icindeki tum parcalari serbest birakir. */
void	lex_free(t_token *tokens)
{
	t_token	*next;

	while (tokens != NULL)
	{
		next = tokens->next;
		seg_free(tokens->segs);
		free(tokens);
		tokens = next;
	}
}

/* Satiri token listesine cevirir; hata halinde NULL doner ve err dolar. */
t_token	*lex_split(const char *line, t_lex_err *err)
{
	t_lexer	lex;

	lex_init(&lex, line, err);
	while (lex.src[lex.i] != '\0')
	{
		if (is_blank(lex.src[lex.i]))
		{
			lex.i++;
			continue ;
		}
		if (is_op_start(lex.src[lex.i]))
		{
			if (read_op(&lex) == 0)
				return (lex_abort(&lex));
			continue ;
		}
		if (read_word(&lex) == 0)
			return (lex_abort(&lex));
	}
	buf_free(&lex.buf);
	return (lex.head);
}
