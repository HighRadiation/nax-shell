/*
** parse.h — sozcuk ayirma, ayristirma ve genisletme bildirimleri.
**
** NEDEN AYRI BASLIK:
**   Kabugun dil tarafi (lexer, ayristirici, genisletme) bir arada calisir
**   ve ortak tipleri paylasir. Bunlari nax.h'a doldurmak yerine kendi
**   basliginda tutmak her modulun neye baktigini gorunur kiliyor.
*/

#ifndef PARSE_H
# define PARSE_H

# include <stddef.h>

/*
** Bir sozcuk parcasinin tirnak kipi.
**
**   Q_NONE   : genislet ve alan ayirmaya tabi tut
**   Q_DOUBLE : genislet ama alan ayirma
**   Q_SINGLE : hicbir sey yapma, harfi harfine al
**
** NEDEN BOYLE MODELLENDI:
**   Tirnaklarin nerede basladigi bilgisi yalnizca lexer'da duruyor.
**   Genisletme asamasi tirnaklari yeniden ayristirmak zorunda kalmaz; ne
**   yapacagini parcanin kipinden okur. Kacis karakteri bir karakteri
**   Q_SINGLE'a dusurur, yani "\$HOME" ile '$HOME' ayni sonucu verir.
**   Ayni mantikla cift tirnak icindeki "\$" o karakteri kendi parcasina
**   ayirir ve Q_SINGLE yapar.
**
** Bu ayrim bos dize sorununu da cozer: "" gercek bir bos argumandir
** (tek parca, Q_DOUBLE, bos metin), ama genislemeden gelen bos deger
** arguman uretmez.
*/
typedef enum e_quote
{
	Q_NONE,
	Q_DOUBLE,
	Q_SINGLE
}	t_quote;

/* Bir sozcugun tek tirnak kipine sahip parcasi. */
typedef struct s_seg
{
	char			*text;
	t_quote			quote;
	struct s_seg	*next;
}	t_seg;

typedef enum e_tok
{
	T_WORD,
	T_PIPE,
	T_REDIR_IN,
	T_REDIR_OUT,
	T_APPEND,
	T_HEREDOC,
	T_SEMI,
	T_AND_IF,
	T_OR_IF,
	T_AMP,
	T_LPAREN,
	T_RPAREN
}	t_tok;

/* Tek bir token; segs yalnizca T_WORD icin doludur. */
typedef struct s_token
{
	t_tok			type;
	t_seg			*segs;
	struct s_token	*next;
}	t_token;

/*
** Sozcuk ayirma hatasi.
**
** message sabit bir metne isaret eder, sahiplik devretmez.
** at, hatanin girdideki sifir tabanli konumudur.
*/
typedef struct s_lex_err
{
	const char	*message;
	size_t		at;
}	t_lex_err;

t_token		*lex_split(const char *line, t_lex_err *err);
void		lex_free(t_token *tokens);
const char	*lex_name(t_tok type);
char		*lex_dump(const t_token *tokens);

#endif
