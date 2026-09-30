/*
** test_lexer.c — sozcuk ayirici icin tablo tabanli testler.
**
** NEDEN TABLO:
**   Vaka eklemek yeniden derleme gerektirmesin. Vakalar
**   test/cases/lexer.tsv icinde durur; yeni bir kenar durum bulundugunda
**   oraya bir satir eklenir.
**
** BEKLENEN CIKTI:
**   lex_dump'in urettigi kanonik metin. Beklenen alan "!" ile baslarsa o
**   hata mesaji beklenir.
**
** Dosya okuma, kacis cozme ve ozet basma isi test/harness.c icinde.
*/

#include "parse.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

/* Hata beklenen bir vakayi dogrular. */
static void	check_error(t_score *score, int no, const char *input,
		const char *want)
{
	t_lex_err	err;
	t_token		*tokens;

	tokens = lex_split(input, &err);
	if (tokens == NULL && err.message != NULL
		&& strcmp(err.message, want + 1) == 0)
		score->passed++;
	else if (tokens != NULL)
		report_fail(score, no, input, want, "(hata beklendi, ayirma basardi)");
	else
		report_fail(score, no, input, want, err.message);
	lex_free(tokens);
}

/* Basarili ayirma beklenen bir vakayi dogrular. */
static void	check_dump(t_score *score, int no, const char *input,
		const char *want)
{
	t_lex_err	err;
	t_token		*tokens;
	char		*got;

	tokens = lex_split(input, &err);
	if (tokens == NULL && err.message != NULL)
	{
		report_fail(score, no, input, want, err.message);
		return ;
	}
	got = lex_dump(tokens);
	if (got != NULL && strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
	free(got);
	lex_free(tokens);
}

/* Bir vakayi turune gore ilgili dogrulayiciya yonlendirir. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	if (want[0] == '!')
		check_error(score, no, input, want);
	else
		check_dump(score, no, input, want);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*path;

	path = "test/cases/lexer.tsv";
	if (argc > 1)
		path = argv[1];
	return (harness_run(path, "lexer testleri", run_case));
}
