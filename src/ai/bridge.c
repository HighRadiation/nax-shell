/*
** bridge.c - kabugun yardimci sureci nasil kullandigi.
**
** NEDEN AYRI MODUL:
**   main.c okuma dongusu, naxd.c surec yonetimi. Arada bir karar katmani
**   var: cevap gelince ne basilir, ne tampona konur, durum kodu ne olur.
**   Bunlar kabugun davranisi, surec yonetimi degil.
**
** BAGLANTI ILK IHTIYACTA KURULUR:
**   Her kabuk acilisinda bir Python sureci baslatmak, AI hic
**   kullanilmayacak oturumlarda bedava maliyet olurdu. Ilk niyet
**   satirinda baslatiliyor.
**
** ANAHTAR YOKSA KABUK DUZ KABUK OLUR:
**   Sebep oturumda BIR KEZ yazilir, sonra satir bash'in yaptigi seye
**   duser: ilk sozcuk icin "command not found" ve 127. Bu, eldeki en
**   bilgilendirici davranis - satiri sessizce yutmak kullaniciyi neyin
**   olmadigi konusunda karanlikta birakirdi.
**
** RISK KARARINI KABUK KENDI VERIYOR:
**   Yardimci surec bir "danger" ipucu gonderiyor ama tek basina ona
**   guvenilmiyor. Kabuk komutun basini kendi listesiyle de denetliyor ve
**   IKISINDEN BIRI riskli diyorsa oneri duzenleme satirina konmuyor.
**   Guvenligi karsi tarafin dogru cevap vermesine baglamak yanlis olurdu.
*/

#include "nax.h"
#include "ai.h"
#include "naxd.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AI_ARGV_MAX 8

static t_naxd	g_ai;
static int		g_started;
static char		*g_words[AI_ARGV_MAX];

/* Gosterge: bekleme uzadiginda bir kez basilir. */
static void	on_slow(void)
{
	fflush(stdout);
	fprintf(stderr, "%s: dusunuyor...\n", NAX_NAME);
}

/*
** Yardimci sureci baslatacak komutu kurar.
**
** NAX_NAXD ortam degiskeni verildiyse o kullanilir; testler sahte bir
** surece yoneltmek icin bunu kullaniyor. Varsayilan, depo kokundeki
** gercek surec.
*/
static char	*const	*build_command(void)
{
	char	*given;
	size_t	n;

	given = getenv("NAX_NAXD");
	if (given == NULL || *given == '\0')
	{
		g_words[0] = (char *)"python3";
		g_words[1] = (char *)"naxd/naxd.py";
		g_words[2] = NULL;
		return (g_words);
	}
	given = strdup(given);
	if (given == NULL)
		return (NULL);
	n = 0;
	g_words[n] = strtok(given, " ");
	while (g_words[n] != NULL && n + 1 < AI_ARGV_MAX)
	{
		n++;
		g_words[n] = strtok(NULL, " ");
	}
	return (g_words);
}

/* Gunluk dosyasinin yolu; yardimci surecin hata cikisi buraya gider. */
static const char	*log_path(void)
{
	static char	path[512];
	const char	*home;

	home = getenv("HOME");
	if (home == NULL || *home == '\0')
		return ("/tmp/nax-naxd.log");
	snprintf(path, sizeof(path), "%s/.nax-naxd.log", home);
	return (path);
}

/*
** Baglantiyi gerekiyorsa kurar; konusmaya hazirsa 1 doner.
**
** Basarisizlikta sebep oturumda bir kez yazilir. Her satirda yeniden
** yazmak, duz kabuk olarak calismaya devam eden bir oturumu
** kullanilamaz hale getirirdi.
*/
static int	ensure_ready(t_shell *sh)
{
	char	*const	*argv;

	(void)sh;
	if (g_started == 0)
	{
		argv = build_command();
		if (argv == NULL || argv[0] == NULL)
			return (0);
		naxd_init(&g_ai, argv, log_path());
		g_started = 1;
	}
	if (naxd_alive(&g_ai))
		return (1);
	if (naxd_open(&g_ai) == 0)
	{
		if (g_ai.warned == 0)
		{
			g_ai.warned = 1;
			ex_warn("AI baslatilamadi; kabuk duz kabuk olarak calisiyor");
		}
		return (0);
	}
	if (g_ai.has_key == 0)
	{
		if (g_ai.warned == 0)
		{
			g_ai.warned = 1;
			ex_warn("AI kapali: nax.conf icinde api_key bos");
		}
		return (0);
	}
	return (1);
}

/* Metnin ilk sozcugunu kopyalar; cagiran serbest birakir. */
static char	*first_word(const char *text)
{
	size_t	n;

	while (*text == ' ' || *text == '\t')
		text++;
	n = 0;
	while (text[n] != '\0' && text[n] != ' ' && text[n] != '\t')
		n++;
	return (strndup(text, n));
}

/*
** AI yoksa satiri bash'in yaptigi gibi bildirir.
**
** Ilk sozcuk icin "command not found" ve 127: satir calistirilmadi ve
** sebebi gorunuyor.
*/
static void	fall_back(t_shell *sh, const char *text)
{
	char	*head;

	head = first_word(text);
	if (head != NULL && *head != '\0')
		ex_warn_name(head, "command not found");
	free(head);
	sh->last_status = 127;
}

/* Onerilen komut geri donusu olmayanlardan biri mi. */
static int	suggestion_is_risky(const t_frame *reply, const char *cmd)
{
	const char	*hint;
	char		*head;
	int		risky;

	hint = proto_field(reply, "danger");
	if (hint != NULL && strcmp(hint, "0") != 0)
		return (1);
	head = first_word(cmd);
	risky = 0;
	if (head != NULL)
		risky = fix_is_dangerous(head);
	free(head);
	return (risky);
}

/*
** Komut onerisini yazar ve uygunsa bir sonraki prompta hazirlar.
**
** Riskli komut yazilir ama tampona KONULMAZ; gerekcesi yerel yazim
** duzeltmesindekiyle ayni, tampona konan oneri tek Enter'la kosar.
*/
static void	show_request(t_shell *sh, const t_frame *reply)
{
	const char	*cmd;

	cmd = proto_field(reply, "cmd");
	if (cmd == NULL || *cmd == '\0')
	{
		ex_warn("AI bos bir komut dondurdu");
		sh->last_status = 1;
		return ;
	}
	fflush(stdout);
	fprintf(stderr, "%s: %s\n", NAX_NAME, cmd);
	sh->last_status = 0;
	if (suggestion_is_risky(reply, cmd) || sh->interactive == 0)
		return ;
	ln_preload(cmd);
}

/* Soru cevabini ekrana basar. */
static void	show_question(t_shell *sh, const t_frame *reply)
{
	const char	*text;

	text = proto_field(reply, "text");
	if (text == NULL)
		text = "";
	printf("%s\n", text);
	sh->last_status = 0;
}

/* Gelen cevabi yoluna gore isler. */
static void	use_reply(t_shell *sh, const t_frame *reply)
{
	const char	*kind;

	kind = proto_field(reply, "kind");
	if (kind != NULL && strcmp(kind, "request") == 0)
		show_request(sh, reply);
	else
		show_question(sh, reply);
}

/* Basarisiz sonucu tek satir olarak bildirir. */
static void	report_trouble(t_shell *sh, t_askst state, const t_frame *reply)
{
	const char	*message;

	if (state == ASK_CANCEL)
	{
		sh->last_status = 130;
		return ;
	}
	if (state == ASK_ERR)
	{
		message = proto_field(reply, "message");
		if (message == NULL)
			message = "bilinmeyen hata";
		ex_warn_name("AI", message);
	}
	else if (state == ASK_TIMEOUT)
		ex_warn("AI cevap vermedi");
	else
		ex_warn("AI baglantisi koptu");
	sh->last_status = 1;
}

/* Istegi gonderip cevabini isler. */
static void	ask_and_use(t_shell *sh, t_ftype type, const t_pair *fields,
		size_t n)
{
	t_frame	reply;
	t_askst	state;

	proto_blank(&reply);
	state = naxd_ask(&g_ai, type, fields, n, &reply, on_slow);
	if (state == ASK_OK)
		use_reply(sh, &reply);
	else
		report_trouble(sh, state, &reply);
	proto_free(&reply);
}

/* Dogal dil satirini yardimci surece goturur. */
void	ai_intent(t_shell *sh, const char *text)
{
	t_pair	fields[3];
	char	cwd[512];
	char	status[16];

	if (ensure_ready(sh) == 0)
	{
		fall_back(sh, text);
		return ;
	}
	fields[0].key = "text";
	fields[0].value = text;
	fields[1].key = "cwd";
	fields[1].value = "";
	if (getcwd(cwd, sizeof(cwd)) != NULL)
		fields[1].value = cwd;
	snprintf(status, sizeof(status), "%d", sh->last_status);
	fields[2].key = "status";
	fields[2].value = status;
	ask_and_use(sh, FR_INTENT, fields, 3);
}

/*
** Son basarisiz komutu aciklatir.
**
** Aciklanacak bir sey yoksa sessiz kalmak yerine soyluyor: kullanici
** tek basina "?" yazdiginda bir cevap bekliyor.
**
** SON KOMUT BASARILIYSA SORULMAZ. Basarili bir komutu "neden
** basarisiz oldu" diye sormak modelden uydurma bir cevap almak demek;
** ustelik bedava degil.
*/
void	ai_explain(t_shell *sh)
{
	t_pair	fields[2];
	char	code[16];

	if (sh->last_cmd == NULL || *sh->last_cmd == '\0')
	{
		ex_warn("aciklanacak bir komut yok");
		sh->last_status = 1;
		return ;
	}
	if (sh->last_status == 0)
	{
		ex_warn("son komut basarili oldu, aciklanacak hata yok");
		return ;
	}
	if (ensure_ready(sh) == 0)
		return ;
	fields[0].key = "cmd";
	fields[0].value = sh->last_cmd;
	snprintf(code, sizeof(code), "%d", sh->last_status);
	fields[1].key = "code";
	fields[1].value = code;
	ask_and_use(sh, FR_EXPLAIN, fields, 2);
}

/* Baglantiyi kapatir; kabuk cikarken cagrilir. */
void	ai_stop(void)
{
	if (g_started == 0)
		return ;
	naxd_close(&g_ai);
	g_started = 0;
}
