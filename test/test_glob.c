/*
** test_glob.c - tek bilesen icin desen eslesmesi.
**
** GIRDI BICIMI:
**   <desen>|<maske>|<ad>
**
**   Maske desenle AYNI uzunlukta olmak zorunda: her karakter icin "1"
**   harfi harfine, "0" desen demek. Maskeyi elle yazmak zahmetli ama
**   kasten boyle: olculen sey tam olarak bu ayrim ve gizlemek testin
**   degerini dusururdu.
**
** BEKLENEN: "eslesti" ya da "eslesmedi".
**
** NEDEN MASKE TESTIN MERKEZINDE:
**   Ayni desen metni, tirnakli yazildiginda harf olmak zorunda. "echo
**   "*.c"" satirinda yildiz bir dosya adi karakteri; "echo *.c"
**   satirinda desen. Ikisini ayiran tek sey maske.
*/

#include "nax.h"
#include "parse.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Eslesme vakasi. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	char		*mask;
	char		*name;
	const char	*got;

	mask = strchr(input, '|');
	if (mask == NULL)
	{
		report_fail(score, no, input, want, "desen|maske|ad bekleniyor");
		return ;
	}
	*mask++ = '\0';
	name = strchr(mask, '|');
	if (name == NULL)
	{
		report_fail(score, no, input, want, "desen|maske|ad bekleniyor");
		return ;
	}
	*name++ = '\0';
	if (strlen(mask) != strlen(input))
	{
		report_fail(score, no, input, want, "maske desenle ayni uzunlukta degil");
		return ;
	}
	if (glob_match(input, mask, name))
		got = "eslesti";
	else
		got = "eslesmedi";
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*given;

	given = "test/cases/glob.tsv";
	if (argc > 1)
		given = argv[1];
	return (harness_run(given, "glob testleri", run_case));
}
