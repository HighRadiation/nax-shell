/*
** test_ctx.c - oturum baglami halkasi.
**
** GIRDI BICIMI:
**   Komutlar "|" ile ayrilir. Her komut "metin" ya da "metin:kod"
**   bicimindedir; kod verilmezse sifir sayilir.
**
** BEKLENEN BICIMI:
**   ";" ile ayrilmis "[kod] metin" kayitlari, ESKIDEN YENIYE. Halkada
**   hicbir sey kalmadiysa "yok".
**
** "D:liste|dizin" vakalari AI'in kapali oldugu dizin eslesmesini olcuyor;
** beklenen "kapali" ya da "acik".
**
** NEDEN SIRA OLCULUYOR:
**   Dil modelleri sonda olana daha cok dikkat eder ve son komut en
**   degerli olan. Sira ters olsa baglam yaniltici olurdu.
*/

#include "nax.h"
#include "ai.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
** Iki noktadan sonrasi tamami rakam mi.
**
** Cikis kodu ayraci olarak iki nokta kullaniliyor ama komutlarin kendisi
** de iki nokta icerebiliyor: "https://ad:parola@sunucu" satiri ilk
** yazimda "parola@sunucu" kismini cikis kodu saniyordu. Ayrac ancak
** sonrasi tamami rakamsa ayrac.
*/
static int	all_digits(const char *at)
{
	if (*at == '\0')
		return (0);
	while (*at != '\0')
	{
		if (*at < '0' || *at > '9')
			return (0);
		at++;
	}
	return (1);
}

/* Girdiyi komutlara bolup halkaya ekler. */
static void	feed(char *input)
{
	char	*part;
	char	*colon;
	int		code;

	ctx_clear();
	part = strtok(input, "|");
	while (part != NULL)
	{
		colon = strrchr(part, ':');
		code = 0;
		if (colon != NULL && all_digits(colon + 1))
		{
			code = atoi(colon + 1);
			*colon = '\0';
		}
		ctx_add(part, code);
		part = strtok(NULL, "|");
	}
}

/* Baglam blogundan yalnizca son komutlar bolumunu cikarir. */
static void	only_recent(const char *block, char *out, size_t cap)
{
	const char	*at;
	const char	*line;
	int			first;

	out[0] = '\0';
	at = strstr(block, "son komutlar:\n");
	if (at == NULL)
	{
		test_append(out, cap, "yok");
		return ;
	}
	line = at + strlen("son komutlar:\n");
	first = 1;
	while (*line == ' ')
	{
		if (first == 0)
			test_append(out, cap, ";");
		first = 0;
		while (*line == ' ')
			line++;
		while (*line != '\0' && *line != '\n')
		{
			test_append(out, cap, (char []){*line, '\0'});
			line++;
		}
		if (*line == '\n')
			line++;
	}
}

/* Halka vakasi. */
static void	case_ring(t_score *score, int no, char *input, char *want)
{
	char	got[1024];
	char	*block;
	char	copy[512];

	snprintf(copy, sizeof(copy), "%s", input);
	feed(copy);
	block = ctx_block();
	if (block == NULL)
	{
		report_fail(score, no, input, want, "baglam uretilemedi");
		return ;
	}
	only_recent(block, got, sizeof(got));
	free(block);
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
}

/*
** Sir halkaya GIRERKEN temizlenmeli.
**
** Olculen sey yalnizca ciktinin temiz olmasi degil: temizlenmemis bir
** sir bellekte hic durmamali. Blok iki kez uretilip ayni sonucu
** vermesi, maskelemenin gonderme aninda degil giriste yapildigini
** gosteriyor.
*/
static int	check_secret_never_stored(void)
{
	char	*first;
	char	*second;
	int		ok;

	ctx_clear();
	ctx_add("export K=gsk_abcdefghij1234567890ABCDEFGHIJ", 0);
	first = ctx_block();
	second = ctx_block();
	if (first == NULL || second == NULL)
		return (free(first), free(second), 0);
	ok = (strstr(first, "gsk_abcdefghij") == NULL);
	ok = ok && (strstr(first, "[GIZLI:anahtar]") != NULL);
	ok = ok && (strcmp(first, second) == 0);
	free(first);
	free(second);
	return (ok);
}

/* Temizlemeden sonra halkada hicbir sey kalmamali. */
static int	check_clear_empties(void)
{
	char	*block;
	int		ok;

	ctx_clear();
	ctx_add("echo bir", 0);
	ctx_clear();
	block = ctx_block();
	if (block == NULL)
		return (0);
	ok = (strstr(block, "son komutlar") == NULL);
	free(block);
	return (ok);
}

/*
** "nax ctx" ciktisi gidecek baytlari gostermeli.
**
** Gizlilik iddiasini denetlenebilir kilan tek ozellik bu, o yuzden
** gosterilen blogun gonderilen blogu ICERMESI olculuyor.
*/
static int	check_dump_contains_block(void)
{
	char	*block;
	char	*dump;
	int		ok;

	ctx_clear();
	ctx_add("echo kanit", 0);
	block = ctx_block();
	dump = ctx_dump();
	if (block == NULL || dump == NULL)
		return (free(block), free(dump), 0);
	ok = (strstr(dump, block) != NULL);
	free(block);
	free(dump);
	return (ok);
}

/* Adi verilen kontrolu kosar. */
static void	case_check(t_score *score, int no, char *input, char *want)
{
	int	ok;

	if (strcmp(input, "secret_never_stored") == 0)
		ok = check_secret_never_stored();
	else if (strcmp(input, "clear_empties") == 0)
		ok = check_clear_empties();
	else if (strcmp(input, "dump_contains_block") == 0)
		ok = check_dump_contains_block();
	else
	{
		report_fail(score, no, input, want, "boyle bir kontrol yok");
		return ;
	}
	if (ok)
		score->passed++;
	else
		report_fail(score, no, input, want, "kontrol basarisiz");
}

/*
** Kapali dizin eslesmesi vakasi.
**
** Girdi "liste|dizin", beklenen "kapali" ya da "acik". En kritik kural
** onek eslesmesi: "/tmp" listesi "/tmpfoo" dizinini KAPATMAMALI, yoksa
** liste istemeden komsu dizinleri de kapatirdi.
*/
static void	case_dir(t_score *score, int no, char *input, char *want)
{
	char		*bar;
	const char	*got;

	bar = strchr(input, '|');
	if (bar == NULL)
	{
		report_fail(score, no, input, want, "liste|dizin bekleniyor");
		return ;
	}
	*bar = '\0';
	if (ai_dir_blocked(input, bar + 1))
		got = "kapali";
	else
		got = "acik";
	*bar = '|';
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
}

/* Vakayi onekine gore yonlendirir. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	if (strncmp(input, "C:", 2) == 0)
		case_check(score, no, input + 2, want);
	else if (strncmp(input, "D:", 2) == 0)
		case_dir(score, no, input + 2, want);
	else
		case_ring(score, no, input, want);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*given;

	given = "test/cases/ctx.tsv";
	if (argc > 1)
		given = argv[1];
	return (harness_run(given, "ctx testleri", run_case));
}
