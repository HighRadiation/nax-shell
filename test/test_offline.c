/*
** test_offline.c - elle yazilmis niyet tablosu.
**
** GIRDI: dogal dil satiri. BEKLENEN: tablodan donen komut ya da "yok".
**
** KORPUSUN YARISI NEGATIF VAKA. Tablonun temkinli olmasi gerekiyor:
** yanlis bir eslesme kullaniciya yanlis komut onermek demek, oysa
** eslesmemenin bedeli yalnizca bir ag turu. Bu yuzden yanlis pozitif
** aramak, dogru pozitif aramak kadar onemli.
*/

#include "nax.h"
#include "ai.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

/* Tablo vakasi. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	const char	*got;

	got = offline_lookup(input);
	if (got == NULL)
		got = "yok";
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*given;

	given = "test/cases/offline.tsv";
	if (argc > 1)
		given = argv[1];
	return (harness_run(given, "offline testleri", run_case));
}
