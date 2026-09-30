/*
** ai.h - AI tarafinin tipleri ve bildirimleri.
**
** NEDEN AYRI BASLIK:
**   nax.h kabugun ortak cekirdegi, parse.h dilin tarafi, exec.h
**   calistirma. AI tarafi dorduncu bir alan: satirin komut mu niyet mi
**   oldugunu karara baglar ve yardimci surecle konusur.
*/

#ifndef AI_H
# define AI_H

# include "parse.h"

/*
** Bir satirin gidecegi yol.
**
** ROUTE_EMPTY      hicbir sey yapilmaz (bos satir, terminal coplugu)
** ROUTE_SHELL      kabuk komutu: ayristirilir ve calistirilir
** ROUTE_INTENT     dogal dil: AI'a gider
** ROUTE_EXPLAIN    son hatanin aciklanmasi istendi
** ROUTE_SYNTAX_ERR gercek sozdizimi hatasi; mesaji lex hatasinda
*/
typedef enum e_route
{
	ROUTE_EMPTY,
	ROUTE_SHELL,
	ROUTE_INTENT,
	ROUTE_EXPLAIN,
	ROUTE_SYNTAX_ERR
}	t_route;

/*
** Sekil vetolari.
**
** Bas PATH'te cozulse bile satirin dogal dil oldugunu gosteren isaretler.
** Hangisinin tetiklendigi kararda tutulur; testler bunu dogruluyor ve
** ileride karar gunlugune yazilacak.
**
** VETO_QUESTION  satir soru isaretiyle bitiyor
** VETO_NONASCII  bir sozcukte ASCII disi harf var
** VETO_WORDBAG   dort ya da daha fazla ciplak kelime, hicbiri yol ya da
**                secenek gorunumunde degil
** VETO_STOPWORD  dogal dile ozgu bir kelime gecti (it, the, hangi...)
** VETO_HEAD      bas belirsiz ve kendi dogrulayicisi basarisiz oldu
*/
# define VETO_QUESTION 0x01
# define VETO_NONASCII 0x02
# define VETO_WORDBAG 0x04
# define VETO_STOPWORD 0x08
# define VETO_HEAD 0x10

/*
** Siniflandirma karari.
**
** route  : satirin gidecegi yol
** text   : islenecek metin. ROUTE_SHELL'de ayristirilacak satir (zorlama
**          onekinden arindirilmis), ROUTE_INTENT'te AI'a gidecek dogal
**          dil. Girdi satirina ISARET EDER, sahibi degildir.
** vetoes : tetiklenen vetolarin bit maskesi
** forced : kullanici bir kacis yolu kullandi mi
*/
typedef struct s_decision
{
	t_route		route;
	const char	*text;
	int			vetoes;
	int			forced;
}	t_decision;

t_decision	cls_classify(const char *line, t_token **tokens,
				t_lex_err *lerr);
int			cls_vetoes(const t_token *tokens, const char *line);
char		*cls_word_text(const t_token *word);
int			cls_has_operator(const t_token *tokens);
size_t		cls_word_count(const t_token *tokens);
const char	*cls_route_name(t_route route);

#endif
