/*
** test_expand.c — genisletme icin tablo tabanli testler.
**
** ZINCIR:
**   Her vaka girdiyi sozcuklere ayirir, ayristirir, sonra genisletir. Yani
**   uc asamanin birlikte calistigini da dogrular.
**
** SABIT ORTAM:
**   Genisletme ortam degiskenlerini okuyor, o yuzden testler kendi
**   ortamini kurar. Degerler kasten kenar durumlari kapsiyor: bos deger,
**   bosluk iceren deger, sonu bosluklu deger, ve tanimsiz bir ad.
**   Son durum degeri de sabit, cunku $? onu okuyor.
**
** BEKLENEN CIKTI:
**   exp_dump'in urettigi kanonik metin. Beklenen alan "!" ile baslarsa o
**   hata mesaji beklenir.
*/

#include "parse.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

/* Vakalarin okudugu sabit kabuk durumu; yalnizca bu dosya kullanir. */
static t_shell	g_shell;

/* Testlerin bagli oldugu sabit ortami kurar. */
static void	setup_env(void)
{
	setenv("TV", "deger", 1);
	setenv("TS", "a b", 1);
	setenv("TE", "", 1);
	setenv("TSP", "a ", 1);
	setenv("HOME", "/test/ev", 1);
	unsetenv("YOK");
	g_shell.last_status = 7;
	g_shell.hist_path = NULL;
	g_shell.interactive = 0;
	g_shell.exiting = 0;
}

/* Girdiyi agaca cevirir; hata halinde bildirip NULL doner. */
static t_cmd	*build(t_score *score, int no, const char *input,
		const char *want, t_token **tokens)
{
	t_lex_err	lerr;
	t_ast_err	aerr;
	t_cmd		*cmds;

	*tokens = lex_split(input, &lerr);
	if (*tokens == NULL && lerr.message != NULL)
	{
		report_fail(score, no, input, want, lerr.message);
		return (NULL);
	}
	cmds = ast_build(*tokens, &aerr);
	if (cmds == NULL && aerr.message != NULL)
	{
		report_fail(score, no, input, want, aerr.message);
		lex_free(*tokens);
		*tokens = NULL;
		return (NULL);
	}
	return (cmds);
}

/* Hata beklenen bir vakayi dogrular. */
static void	check_error(t_score *score, int no, const char *input,
		const char *want, const t_cmd *cmds)
{
	t_exp_err	err;
	char		*got;

	got = exp_dump(cmds, &g_shell, &err);
	if (got == NULL && err.message != NULL
		&& strcmp(err.message, want + 1) == 0)
		score->passed++;
	else if (got != NULL)
		report_fail(score, no, input, want,
			"(hata beklendi, genisletme basardi)");
	else
		report_fail(score, no, input, want, err.message);
	free(got);
}

/* Basarili genisletme beklenen bir vakayi dogrular. */
static void	check_dump(t_score *score, int no, const char *input,
		const char *want, const t_cmd *cmds)
{
	t_exp_err	err;
	char		*got;

	got = exp_dump(cmds, &g_shell, &err);
	if (got != NULL && strcmp(got, want) == 0)
		score->passed++;
	else if (got == NULL)
		report_fail(score, no, input, want, err.message);
	else
		report_fail(score, no, input, want, got);
	free(got);
}

/* Bir vakayi agaca cevirip turune gore dogrulayiciya yonlendirir. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	t_token	*tokens;
	t_cmd	*cmds;

	tokens = NULL;
	cmds = build(score, no, input, want, &tokens);
	if (tokens == NULL && cmds == NULL)
		return ;
	if (want[0] == '!')
		check_error(score, no, input, want, cmds);
	else
		check_dump(score, no, input, want, cmds);
	ast_free(cmds);
	lex_free(tokens);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*path;

	path = "test/cases/expand.tsv";
	if (argc > 1)
		path = argv[1];
	setup_env();
	return (harness_run(path, "expand testleri", run_case));
}
