/*
** parse.h - sozcuk ayirma, ayristirma ve genisletme bildirimleri.
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
/*
** Bir yonlendirme.
**
** body ve raw YALNIZCA "<<" icin anlamli:
**   body  satir satir toplanan ham govde; SAHIPLI, ast_free birakir
**   raw   sinirlayici tirnakli miydi. Tirnakliysa govde genisletilmez;
**         bash boyle davraniyor ve kural "sinirlayicinin tirnagina bak"
**         seklinde, govdenin icine bakmak degil
**
** Govde neden burada: toplama ayristirmadan SONRA yapiliyor, cunku
** okunacak satir sayisi ancak sinirlayici bilindikten sonra belli olur.
*/
typedef struct s_redir
{
	t_tok			type;
	const t_token	*target;
	char			*body;
	int				raw;
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
/*
** Bir boru hattinin ONCEKI hatla baglantisi.
**
** J_FIRST  listenin ilk hatti; kosulsuz kosar
** J_SEMI   ";"  onceki ne yaparsa yapsin kosar
** J_AND    "&&" onceki BASARILI ise kosar
** J_OR     "||" onceki BASARISIZ ise kosar
**
** NEDEN BAGLANTI HATTIN KENDISINDE TUTULUYOR:
**   Alternatif, baglantiyi iki hattin ARASINDA ayri bir kayitta tutmakti.
**   Boyle daha az yapi var ve calistirici tek dongu ile yurutuyor: her
**   hatta gelince "ben kosmali miyim" sorusunu kendi baglantisina bakip
**   yanitliyor.
*/
typedef enum e_join
{
	J_FIRST,
	J_SEMI,
	J_AND,
	J_OR
}	t_join;

/* Bir boru hatti ve onceki hatla baglantisi. */
typedef struct s_pipeline
{
	t_cmd				*cmds;
	t_join				join;
	struct s_pipeline	*next;
}	t_pipeline;

typedef struct s_parser
{
	const t_token	*tok;
	t_cmd			*head;
	t_cmd			*tail;
	t_arg			*arg_tail;
	t_redir			*redir_tail;
	t_pipeline		*list_head;
	t_pipeline		*list_tail;
	t_ast_err		*err;
}	t_parser;

t_token		*lex_split(const char *line, t_lex_err *err);
void		lex_free(t_token *tokens);
const char	*lex_name(t_tok type);
char		*lex_dump(const t_token *tokens);

/*
** Genisletme sonucu olusan tek bir alan.
**
** Bir sozcuk BIR alan uretmek zorunda degil: tirnaksiz bir degiskenin
** degeri bosluk iceriyorsa birden fazla alana bolunur, bos bir degere
** genisleyen tirnaksiz sozcuk ise HIC alan uretmez.
*/
typedef struct s_field
{
	char			*text;
	struct s_field	*next;
}	t_field;

/* Genisletilmis yonlendirme: turu ve cozulmus dosya adi. */
/*
** Genisletilmis yonlendirme.
**
** path alani "<<" icin DOSYA YOLU DEGIL, gonderilecek govde metnidir.
** Ayri bir alan eklemek yerine ayni alanin kullanilmasinin sebebi: her
** iki halde de "bu yonlendirmenin verisi" anlamini tasiyor ve
** calistirici tipe bakip ne yapacagini zaten biliyor.
*/
typedef struct s_xredir
{
	t_tok			type;
	char			*path;
	struct s_xredir	*next;
}	t_xredir;

/*
** Bir komutun genisletilmis hali.
**
** Boru hattinin tamami icin paralel bir agac KURULMAZ; calistirici her
** komutu calistirmadan hemen once genisletir, kullanir ve birakir. Bu
** yuzden burada next alani yok.
**
** args ve redirs bu yapinin SAHIP oldugu bellektir; xcmd_free birakir.
*/
typedef struct s_xcmd
{
	t_field		*args;
	t_xredir	*redirs;
}	t_xcmd;

/*
** Genisletme hatasi.
**
** message sabit bir metne isaret eder, sahiplik devretmez.
** at, hataya yol acan sozcuk token'idir.
*/
typedef struct s_exp_err
{
	const char		*message;
	const t_token	*at;
}	t_exp_err;

/*
** Genisletme sirasindaki tum gecici durum.
**
** Yalnizca expand.c kullanir; yapi tanimlari norm geregi basliklarda durur.
**
** sh         : degisken ve son durum degerlerinin okundugu kabuk
** word       : genisletilmekte olan sozcuk; hata bildiriminde kullanilir
** buf        : uzerinde calisilan alanin metni burada birikir
** head, tail : uretilmekte olan alan listesi
** emit       : SIRADAKI alan bos kalsa bile uretilmeli mi
** err        : hata bildirimi icin cagiranin verdigi yer
**
** emit SOZCUK BASINA DEGIL ALAN BASINA tutulur ve her alan uretildiginde
** sifirlanir. Gerekcesi: TSP="a " iken "$TSP\"\"" iki alan uretir, [a] ve
** bos olan. Bayrak sozcuk basina olsaydi ikinci alan kaybolurdu. Bash'in
** davranisi olculerek dogrulandi.
*/
typedef struct s_expander
{
	const t_shell	*sh;
	const t_token	*word;
	t_buf			buf;
	t_field			*head;
	t_field			*tail;
	int				emit;
	t_exp_err		*err;
}	t_expander;

t_pipeline	*ast_build(const t_token *tokens, t_ast_err *err);
int			heredoc_fill(t_pipeline *list, const t_shell *sh);
int			exp_heredoc(const t_redir *redir, const t_shell *sh,
				char **out);
int			exp_raw_text(const char *text, const t_shell *sh,
				char **out, t_exp_err *err);
void		ast_free(t_pipeline *list);
char		*ast_dump(const t_pipeline *list);

t_field		*exp_word(const t_token *word, const t_shell *sh,
				t_exp_err *err);
int			exp_cmd(const t_cmd *cmd, const t_shell *sh, t_xcmd *out,
				t_exp_err *err);
void		field_free(t_field *fields);
void		xcmd_free(t_xcmd *xcmd);
char		*exp_dump(const t_cmd *cmds, const t_shell *sh, t_exp_err *err);

#endif
