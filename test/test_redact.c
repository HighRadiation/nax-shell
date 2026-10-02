/*
** test_redact.c - gonderilecek metinden sirlarin temizlenmesi.
**
** IKI YONLU OLCUM:
**   Maskelemenin yakalamasi kadar YAKALAMAMASI da olculuyor. Yanlis
**   maskeleme sessizce yanlis cevap uretir: "mkdir -p dizin" satirinda
**   dizin adi parola sanilirsa model komutu hic anlamaz. Bu yuzden
**   korpusun yarisi negatif vaka.
**
** VAKA TURLERI:
**   <metin>     Maskelenir; beklenen tam cikti.
**   N:<ad>      Ortam degiskeni ADI sir mi; beklenen "sir" ya da "temiz".
**   C:<ad>      Tabloda anlatilamayan kontrol.
**
** COK SATIRLI VAKALAR TABLODA DEGIL: iskele "\n" kacisini bilincli olarak
** tanimiyor (sozcuk ayirici korpusunda "\n" gercek bir ters bolu demek).
** Ozel anahtar blogunun satirlara yayilmis hali C icinde olculuyor.
*/

#include "nax.h"
#include "ai.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maskeleme vakasi. */
static void	case_text(t_score *score, int no, char *input, char *want)
{
	char	*got;

	got = redact_text(input);
	if (got == NULL)
	{
		report_fail(score, no, input, want, "bellek ayrilamadi");
		return ;
	}
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
	free(got);
}

/* Ortam degiskeni adi vakasi. */
static void	case_name(t_score *score, int no, char *input, char *want)
{
	const char	*got;

	if (redact_is_secret_name(input))
		got = "sir";
	else
		got = "temiz";
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
}

/*
** Satirlara yayilmis ozel anahtar blogu tek yer tutucuya inmeli.
**
** Blogun ortasindaki satirlar base64 oldugu icin tek tek de
** maskelenirdi, ama o zaman cikti bir sira yer tutucu olurdu ve metnin
** bir ANAHTAR BLOGU oldugu bilgisi kaybolurdu.
*/
static int	check_multiline_block(void)
{
	const char	*text;
	char		*got;
	int			ok;

	text = "cat anahtar\n-----BEGIN RSA PRIVATE KEY-----\n"
		"MIIEowIBAAKCAQEAxyzabc123\nQIDAQAB\n"
		"-----END RSA PRIVATE KEY-----\nbitti";
	got = redact_text(text);
	if (got == NULL)
		return (0);
	ok = (strcmp(got, "cat anahtar\n[GIZLI:ozel-anahtar]\nbitti") == 0);
	free(got);
	return (ok);
}

/*
** Bitis satiri olmayan blok sonuna kadar maskelenir.
**
** Yarim bir blok yine bir sirdir; "bitis yok" diye oldugu gibi
** gondermek en kotu davranis olurdu.
*/
static int	check_unterminated_block(void)
{
	char	*got;
	int		ok;

	got = redact_text("-----BEGIN PRIVATE KEY-----\nMIIEowIBAAKCAQEA");
	if (got == NULL)
		return (0);
	ok = (strcmp(got, "[GIZLI:ozel-anahtar]") == 0);
	free(got);
	return (ok);
}

/* NULL girdi bos dize vermeli; cagiran her yerde denetim yapmasin. */
static int	check_null_input(void)
{
	char	*got;
	int		ok;

	got = redact_text(NULL);
	if (got == NULL)
		return (0);
	ok = (got[0] == '\0');
	free(got);
	return (ok);
}

/* Adi verilen kontrolu kosar. */
static void	case_check(t_score *score, int no, char *input, char *want)
{
	int	ok;

	if (strcmp(input, "multiline_block") == 0)
		ok = check_multiline_block();
	else if (strcmp(input, "unterminated_block") == 0)
		ok = check_unterminated_block();
	else if (strcmp(input, "null_input") == 0)
		ok = check_null_input();
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

/* Vakayi onekine gore yonlendirir. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	if (strncmp(input, "N:", 2) == 0)
		case_name(score, no, input + 2, want);
	else if (strncmp(input, "C:", 2) == 0)
		case_check(score, no, input + 2, want);
	else
		case_text(score, no, input, want);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*given;

	given = "test/cases/redact.tsv";
	if (argc > 1)
		given = argv[1];
	return (harness_run(given, "redact testleri", run_case));
}
