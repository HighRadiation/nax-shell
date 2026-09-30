/*
** classifier.c - satir komut mu niyet mi.
**
** Projenin kalbi burasi. Kullanici ozel bir isaret yazmadigi icin her
** satir icin bu karari kabuk kendi vermek zorunda.
**
** KARAR SIRASI, ucuzdan pahaliya; ilk eslesen kazanir:
**
**   0  On filtre    bos satir, denetim karakteri coplugu, kacis onekleri
**   1  Sozcuklere ayirma
**   2  Bas cozumu   yerlesik mi, PATH'te mi
**   3  Sekil vetosu bas cozuldu ama satir dogal dil mi (veto.c)
**   4  Niyet
**
** ASIL SONUC: normal komutlar AI'a HIC ugramaz. "ls -la" yazildiginda ne
** gecikme ne ucret olusur; AI yalnizca kabugun anlamlandiramadigi
** satirlarda devreye girer.
**
** HENUZ YOK:
**   Yerel yazim duzeltmesi. Bas cozulmezse tek sozcuklu satir kabuk
**   yoluna gidiyor ve 127 aliyor; sonraki adimda "celar" gibi satirlar
**   AI'a hic gitmeden yerelde duzeltme onerisi alacak. Gerekcesi
**   unresolved_head'in basinda.
*/

#include "nax.h"
#include "ai.h"
#include "exec.h"
#include <stdlib.h>
#include <string.h>

/* Bastaki bosluklari atlar. */
static const char	*skip_blank(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}

/*
** Satirda terminalin sizdirdigi denetim karakteri var mi.
**
** NEDEN VAR: terminal bazen fare raporu gibi CSI dizilerini stdin'e
** sizdiriyor; kullanicinin gecmisinde gercekten boyle satirlar var
** ("0;93;14M0;93;14mexit" gibi). Bunlar siniflandirilmadan dusurulur,
** yoksa her sizan rapor bir AI cagrisina donusurdu.
*/
static int	has_control_char(const char *line)
{
	unsigned char	c;

	while (*line != '\0')
	{
		c = (unsigned char)*line;
		if ((c < 0x20 && c != '\t') || c == 0x7f)
			return (1);
		line++;
	}
	return (0);
}

/*
** Asama 0: ham satiri on filtreden gecirir.
**
** Karar verildiyse 1, satirin devam etmesi gerekiyorsa 0 doner.
**
** KACIS YOLLARI (normal kullanimda hicbiri yazilmaz):
**   tek basina "?"  son hatayi aciklat
**   "\" oneki       kabugu zorla, hicbir veto bakilmaz
*/
static int	prefilter(const char *line, t_decision *d)
{
	const char	*p;

	p = skip_blank(line);
	if (*p == '\0' || has_control_char(line))
	{
		d->route = ROUTE_EMPTY;
		return (1);
	}
	if (p[0] == '?' && *skip_blank(p + 1) == '\0')
	{
		d->route = ROUTE_EXPLAIN;
		return (1);
	}
	if (p[0] == '\\' && p[1] != '\0' && p[1] != ' ' && p[1] != '\t')
	{
		d->text = p + 1;
		d->forced = 1;
	}
	return (0);
}

/*
** Kapanmamis tirnak hatasini yorumlar.
**
** Bu hata hem gercek bir sozdizimi hatasi hem de dogal dilin isareti
** olabilir: "git'e dokunma" ve "don't touch it" iki kesme isaretiyle
** yazilmis cumleler, "echo 'acik" ise gercek bir hata.
**
** AYIRT EDEN SEY: tirnagin hemen oncesinde bosluk olup olmadigi. Bir
** sozcuge YAPISIK kesme isareti neredeyse her zaman iyelik ya da
** kisaltmadir; bosluktan sonra gelen tirnak ise gercekten tirnaktir.
**
**   echo 'acik      -> tirnaktan once bosluk  -> sozdizimi hatasi
**   git'e dokunma   -> tirnaktan once 't'     -> niyet
**   don't touch it  -> tirnaktan once 'n'     -> niyet
*/
static void	on_quote_error(t_decision *d, const char *line,
		const t_lex_err *lerr)
{
	char	before;

	d->route = ROUTE_SYNTAX_ERR;
	if (lerr->at == 0)
		return ;
	before = line[lerr->at - 1];
	if (before != ' ' && before != '\t')
		d->route = ROUTE_INTENT;
}

/* Sozcuk ayirma hatasini yola cevirir. */
static void	on_lex_error(t_decision *d, const t_lex_err *lerr)
{
	if (d->forced == 0 && strncmp(lerr->message, "kapanmamis", 10) == 0)
		on_quote_error(d, d->text, lerr);
	else
		d->route = ROUTE_SYNTAX_ERR;
}

/* Asama 2: ilk sozcuk bir yerlesige ya da PATH'teki bir komuta cozuluyor mu. */
static int	head_resolves(const t_token *tokens)
{
	char	*text;
	int		ok;

	if (tokens == NULL || tokens->type != T_WORD)
		return (0);
	text = cls_word_text(tokens);
	if (text == NULL)
		return (0);
	ok = (bi_lookup(text) != NULL || path_is_command(text));
	free(text);
	return (ok);
}

/*
** Bas cozulmediginde ne yapilacagi.
**
** PLANDAN BILINCLI SAPMA: plan "kalan her sey niyet" diyordu. Oyle
** yapilinca "boylebirkomutyok" artik "command not found" vermiyor, AI'a
** gidiyor - yani kabugun en temel hata mesaji kayboluyor ve yazim
** hatasinin cevabi bir ag turuna baglaniyor.
**
** KURAL: satirda kabuk operatoru varsa ya da tek sozcukse kabuk yoluna
** gider ve calistirici 127 uretir; iki ya da daha fazla sozcukse niyet
** sayilir.
**
** OPERATOR KURALI SONRADAN EKLENDI: olmadan "a && b" ve "> dosya"
** satirlari niyet saniliyor, kabugun sozdizimi hatasi hic gorunmuyordu.
**
** GEREKCE: tek bir sozcuk neredeyse her zaman yazim hatasidir, dogal dil
** istegi degil; ve bash'in cevabi (127) aninda ve dogrudur. Iki sozcukten
** sonrasi ise tersine doner - "sil eski loglari" istek, "boylebirkomutyok
** arg1 arg2" ise yanlis yazilmis komut olabilir, ama ilkine "command not
** found" demek ikincisine "boyle bir komut yok" demekten daha kotu.
**
** Yazim hatasinin yerel cevabi sonraki adimda gelecek; o zaman tek
** sozcuklu satirlar AI'a hic ugramadan duzeltme onerisi alacak.
*/
static t_decision	*unresolved_head(t_decision *d, const t_token *tokens)
{
	if (cls_has_operator(tokens) || cls_word_count(tokens) == 1)
		d->route = ROUTE_SHELL;
	else
		d->route = ROUTE_INTENT;
	return (d);
}

/* Yolun okunabilir adini dondurur; test ve gunluk icin. */
const char	*cls_route_name(t_route route)
{
	if (route == ROUTE_EMPTY)
		return ("EMPTY");
	if (route == ROUTE_SHELL)
		return ("SHELL");
	if (route == ROUTE_INTENT)
		return ("INTENT");
	if (route == ROUTE_EXPLAIN)
		return ("EXPLAIN");
	return ("SYNTAX_ERR");
}

/*
** Satiri siniflandirir.
**
** Basarili ayirma halinde token listesi *tokens'a yazilir ve CAGIRAN
** lex_free ile birakir; aksi halde NULL kalir. Karardaki text girdi
** satirina isaret eder, sahibi degildir.
*/
t_decision	cls_classify(const char *line, t_token **tokens, t_lex_err *lerr)
{
	t_decision	d;

	d.route = ROUTE_SHELL;
	d.text = line;
	d.vetoes = 0;
	d.forced = 0;
	*tokens = NULL;
	lerr->message = NULL;
	lerr->at = 0;
	if (prefilter(line, &d))
		return (d);
	*tokens = lex_split(d.text, lerr);
	if (*tokens == NULL)
	{
		if (lerr->message != NULL)
			on_lex_error(&d, lerr);
		else
			d.route = ROUTE_EMPTY;
		return (d);
	}
	if (d.forced)
		return (d);
	if (head_resolves(*tokens) == 0)
		return (*unresolved_head(&d, *tokens));
	d.vetoes = cls_vetoes(*tokens, d.text);
	if (d.vetoes != 0)
		d.route = ROUTE_INTENT;
	return (d);
}
