/*
** test_parser.c — ayristirici icin tablo tabanli testler.
**
** ZINCIR:
**   Her vaka girdiyi once sozcuklere ayirir, sonra ayristirir. Yani bu
**   testler ayni zamanda iki asamanin birlikte calistigini dogrular.
**
** BEKLENEN CIKTI:
**   ast_dump'in urettigi kanonik metin. Beklenen alan "!" ile baslarsa o
**   hata mesaji beklenir.
*/

#include "parse.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

/* Hata beklenen bir vakayi dogrular. */
static void	check_error(t_score *score, int no, const char *input,
		const char *want, const t_token *tokens)
{
	t_ast_err	err;
	t_cmd		*cmds;

	cmds = ast_build(tokens, &err);
	if (cmds == NULL && err.message != NULL
		&& strcmp(err.message, want + 1) == 0)
		score->passed++;
	else if (cmds != NULL)
		report_fail(score, no, input, want,
			"(hata beklendi, ayristirma basardi)");
	else if (err.message == NULL)
		report_fail(score, no, input, want, "(hicbir sey ayristirilmadi)");
	else
		report_fail(score, no, input, want, err.message);
	ast_free(cmds);
}

/* Basarili ayristirma beklenen bir vakayi dogrular. */
static void	check_dump(t_score *score, int no, const char *input,
		const char *want, const t_token *tokens)
{
	t_ast_err	err;
	t_cmd		*cmds;
	char		*got;

	cmds = ast_build(tokens, &err);
	if (cmds == NULL && err.message != NULL)
	{
		report_fail(score, no, input, want, err.message);
		return ;
	}
	got = ast_dump(cmds);
	if (got != NULL && strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
	free(got);
	ast_free(cmds);
}

/* Bir vakayi sozcuklere ayirip turune gore dogrulayiciya yonlendirir. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	t_lex_err	err;
	t_token		*tokens;

	tokens = lex_split(input, &err);
	if (tokens == NULL && err.message != NULL)
	{
		report_fail(score, no, input, want, err.message);
		return ;
	}
	if (want[0] == '!')
		check_error(score, no, input, want, tokens);
	else
		check_dump(score, no, input, want, tokens);
	lex_free(tokens);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*path;

	path = "test/cases/parser.tsv";
	if (argc > 1)
		path = argv[1];
	return (harness_run(path, "parser testleri", run_case));
}
