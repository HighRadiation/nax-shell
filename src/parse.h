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
# include "nax.h"

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

/*
** Sozcuk ayirma sirasindaki tum gecici durum.
**
** Yalnizca lexer.c kullanir; yapi tanimlari norm geregi basliklarda durur.
**
** src               : ayristirilan satir
** i                 : satirdaki mevcut konum
** head, tail        : uretilmekte olan token listesi
** seg_head, seg_tail: uzerinde calisilan sozcugun parcalari
** buf               : bir sonraki parcanin metni burada birikir
** err               : hata bildirimi icin cagiranin verdigi yer
*/
typedef struct s_lexer
{
	const char	*src;
	size_t		i;
	t_token		*head;
	t_token		*tail;
	t_seg		*seg_head;
	t_seg		*seg_tail;
	t_buf		buf;
	t_lex_err	*err;
}	t_lexer;

/*
** Komutun bir argumani.
**
** word, sozcuk ayiricinin urettigi token listesine ISARET EDER; sahibi
** degildir. Sahiplik kurali agacin tamaminda ayni: agac token'lari
** odunc alir, birakmaz. Gerekcesi parser.c dosyasinin basinda yazili.
*/
typedef struct s_arg
{
	const t_token	*word;
	struct s_arg	*next;
}	t_arg;

/*
** Bir yonlendirme: turu ve hedefi.
**
** type yalnizca T_REDIR_IN, T_REDIR_OUT, T_APPEND ya da T_HEREDOC olur.
** target da odunc alinmis bir sozcuk token'idir.
*/
typedef struct s_redir
{
	t_tok			type;
	const t_token	*target;
	struct s_redir	*next;
}	t_redir;

/*
** Boru hattindaki tek bir komut.
**
** args   : argumanlar, yazildiklari sirada
** redirs : yonlendirmeler, yazildiklari sirada
** next   : boru hattindaki sonraki komut
**
** Bir komut ya en az bir arguman ya da en az bir yonlendirme icermek
** zorunda; ikisi de bos olan komut sozdizimi hatasidir.
*/
typedef struct s_cmd
{
	t_arg			*args;
	t_redir			*redirs;
	struct s_cmd	*next;
}	t_cmd;

/*
** Ayristirma hatasi.
**
** message sabit bir metne isaret eder, sahiplik devretmez.
** at, hataya yol acan token'dir; satir sonunda bitmisse NULL olur.
*/
typedef struct s_ast_err
{
	const char		*message;
	const t_token	*at;
}	t_ast_err;

/*
** Ayristirma sirasindaki tum gecici durum.
**
** Yalnizca parser.c kullanir; yapi tanimlari norm geregi basliklarda durur.
**
** tok        : okunmakta olan token
** head, tail : uretilmekte olan boru hatti
** arg_tail   : uzerinde calisilan komutun son argumani
** redir_tail : uzerinde calisilan komutun son yonlendirmesi
** err        : hata bildirimi icin cagiranin verdigi yer
*/
typedef struct s_parser
{
	const t_token	*tok;
	t_cmd			*head;
	t_cmd			*tail;
	t_arg			*arg_tail;
	t_redir			*redir_tail;
	t_ast_err		*err;
}	t_parser;

t_token		*lex_split(const char *line, t_lex_err *err);
void		lex_free(t_token *tokens);
const char	*lex_name(t_tok type);
char		*lex_dump(const t_token *tokens);

t_cmd		*ast_build(const t_token *tokens, t_ast_err *err);
void		ast_free(t_cmd *cmds);
char		*ast_dump(const t_cmd *cmds);

#endif
