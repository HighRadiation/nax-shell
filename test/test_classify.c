/*
** test_classify.c - siniflandirici icin tablo tabanli testler.
**
** NEDEN KONTROLLU FIKSTUR:
**   Karar calisma dizinine bagli: ASCII disi ve durak kelime vetolari, o
**   sozcuk var olan bir dosyayi adlandiriyorsa tetiklenmiyor. Yani ayni
**   satir, farkli dizinde farkli siniflandirilir - ve bu bir hata degil,
**   tasarim. Testler bu yuzden kendi gecici dizinini kurup icine giriyor.
**
** FIKSTUR ICERIGI (vakalar buna gore yazildi):
**   sirket.txt     var olan sade dosya
**   sirket.txt'in ASCII disi ikizi   ASCII disi istisnasi icin
**   Makefile       make dogrulayicisi gecsin diye
**   those          durak kelime istisnasi icin
**   altdizin/      var olan dizin
**
**   "it", "all", "the" gibi kelimeler BILINCLI olarak dosya DEGIL, cunku
**   bu kelimeleri iceren vakalar vetonun tetiklenmesini bekliyor.
**
** BEKLENEN ALAN BICIMI:
**   Yol adi: SHELL, INTENT, EMPTY, EXPLAIN, SYNTAX_ERR
**   Veto de dogrulanacaksa iki nokta ve virgullu liste:
**     INTENT:HEAD
**     INTENT:NONASCII,STOPWORD
**   Veto listesi verilmezse yalnizca yola bakilir.
*/

#include "nax.h"
#include "ai.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Fikstur dizininin yolu; temizlik icin saklanir. */
static char	g_fixture[256];

/* ASCII disi adi kaynakta kacis dizisiyle yazar; norm kodu ASCII tutuyor. */
static const char	*non_ascii_name(void)
{
	return ("\xc5\x9firket.txt");
}

/* Gecici bir dizin kurup icine girer ve fikstur dosyalarini olusturur. */
static int	setup_fixture(void)
{
	FILE	*fp;

	strcpy(g_fixture, "/tmp/nax_cls_XXXXXX");
	if (mkdtemp(g_fixture) == NULL || chdir(g_fixture) != 0)
		return (0);
	fp = fopen("sirket.txt", "w");
	if (fp != NULL)
		fclose(fp);
	fp = fopen(non_ascii_name(), "w");
	if (fp != NULL)
		fclose(fp);
	fp = fopen("Makefile", "w");
	if (fp != NULL)
		fclose(fp);
	fp = fopen("those", "w");
	if (fp != NULL)
		fclose(fp);
	if (mkdir("altdizin", 0755) != 0)
		return (0);
	return (1);
}

/* Fikstur dizinini siler. */
static void	cleanup_fixture(void)
{
	if (chdir("/") != 0)
		return ;
	unlink("sirket.txt");
	unlink(non_ascii_name());
	unlink("Makefile");
	unlink("those");
}

/* Veto adini bit maskesine cevirir; taninmayan ad icin 0 doner. */
static int	veto_bit(const char *name)
{
	if (strcmp(name, "QUESTION") == 0)
		return (VETO_QUESTION);
	if (strcmp(name, "NONASCII") == 0)
		return (VETO_NONASCII);
	if (strcmp(name, "WORDBAG") == 0)
		return (VETO_WORDBAG);
	if (strcmp(name, "STOPWORD") == 0)
		return (VETO_STOPWORD);
	if (strcmp(name, "HEAD") == 0)
		return (VETO_HEAD);
	return (0);
}

/* Virgulle ayrilmis veto listesini maskeye cevirir. */
static int	veto_mask(char *list)
{
	char	*token;
	int		mask;

	mask = 0;
	token = strtok(list, ",");
	while (token != NULL)
	{
		mask |= veto_bit(token);
		token = strtok(NULL, ",");
	}
	return (mask);
}

/* Beklenen ile geleni karsilastirip sonucu yazar. */
static void	judge(t_score *score, int no, const char *input,
		const char *want, const t_decision *d)
{
	char		copy[256];
	char		got[256];
	const char	*colon;
	int			mask;

	colon = strchr(want, ':');
	if (colon == NULL)
	{
		if (strcmp(cls_route_name(d->route), want) == 0)
			score->passed++;
		else
			report_fail(score, no, input, want, cls_route_name(d->route));
		return ;
	}
	snprintf(copy, sizeof(copy), "%s", colon + 1);
	mask = veto_mask(copy);
	snprintf(got, sizeof(got), "%s:%#x", cls_route_name(d->route), d->vetoes);
	if (strncmp(cls_route_name(d->route), want, (size_t)(colon - want)) == 0
		&& d->vetoes == mask)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
}

/* Bir vakayi siniflandirip beklenenle karsilastirir. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	t_lex_err	err;
	t_token		*tokens;
	t_decision	d;

	d = cls_classify(input, &tokens, &err);
	judge(score, no, input, want, &d);
	lex_free(tokens);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	char		path[4096];
	const char	*given;
	int			code;

	given = "test/cases/classify.tsv";
	if (argc > 1)
		given = argv[1];
	if (realpath(given, path) == NULL)
	{
		printf("  HATA: %s bulunamadi\n", given);
		return (1);
	}
	if (setup_fixture() == 0)
	{
		printf("  HATA: fikstur kurulamadi\n");
		return (1);
	}
	code = harness_run(path, "classify testleri", run_case);
	cleanup_fixture();
	return (code);
}
