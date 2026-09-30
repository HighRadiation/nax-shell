/*
** test_parser.c — ayristirici icin tablo tabanli testler.
**
** NEDEN AYRI KOSUCU:
**   Vakalar test/cases/parser.tsv icinde ve karsilastirma bicimi
**   ast_dump'in urettigi metin. Sozcuk ayirici testlerinden ayri durmasi
**   gerekiyor, cunku ikisi farkli seyi olcuyor: orada tirnak ayrintisi
**   onemli, burada agacin sekli.
**
** ZINCIR:
**   Her vaka girdiyi once sozcuklere ayirir, sonra ayristirir. Yani bu
**   testler ayni zamanda iki asamanin birlikte calistigini dogrular.
**
** KACISLAR VE BICIM:
**   test_lexer.c ile ayni: sekme alan ayiricisi, \t ve \\ cozulur,
**   beklenen alan "!" ile baslarsa o hata mesaji beklenir.
*/

#include "parse.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX_LEN 4096
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define OFF "\033[0m"

/* Metin icindeki \t ve \\ kacislarini yerinde cozer. */
static void	unescape(char *s)
{
	char	*out;

	out = s;
	while (*s != '\0')
	{
		if (*s == '\\' && s[1] == 't')
		{
			*out = '\t';
			s += 2;
		}
		else if (*s == '\\' && s[1] == '\\')
		{
			*out = '\\';
			s += 2;
		}
		else
		{
			*out = *s;
			s++;
		}
		out++;
	}
	*out = '\0';
}

/* Satirdaki ilk sekmeye kadarki alani ayirir ve kalani dondurur. */
static char	*split_tab(char *line)
{
	char	*tab;

	tab = strchr(line, '\t');
	if (tab == NULL)
		return (NULL);
	*tab = '\0';
	return (tab + 1);
}

/* Sondaki yeni satir karakterlerini atar. */
static void	chomp(char *line)
{
	size_t	len;

	len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
	{
		line[len - 1] = '\0';
		len--;
	}
}

/* Basarisiz vakayi beklenen ve gelen degerlerle birlikte basar. */
static void	report_fail(t_score *score, int no, const char *input,
		const char *want, const char *got)
{
	score->failed++;
	printf("  %spatladi%s satir %d\n", RED, OFF, no);
	printf("    girdi    : %s\n", input);
	printf("    beklenen : %s\n", want);
	printf("    gelen    : %s\n", got);
}

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
		report_fail(score, no, input, want, "(hata beklendi, ayristirma basardi)");
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
	t_lex_err	lerr;
	t_token		*tokens;

	unescape(input);
	unescape(want);
	tokens = lex_split(input, &lerr);
	if (tokens == NULL && lerr.message != NULL)
	{
		report_fail(score, no, input, want, lerr.message);
		return ;
	}
	if (want[0] == '!')
		check_error(score, no, input, want, tokens);
	else
		check_dump(score, no, input, want, tokens);
	lex_free(tokens);
}

/* Vaka dosyasini okur ve her satiri kosar. */
static int	run_file(const char *path, t_score *score)
{
	FILE	*fp;
	char	line[LINE_MAX_LEN];
	char	*want;
	int		no;

	fp = fopen(path, "r");
	if (fp == NULL)
	{
		printf("  %sHATA%s: %s acilamadi\n", RED, OFF, path);
		return (0);
	}
	no = 0;
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		no++;
		chomp(line);
		if (line[0] == '#' || line[0] == '\0')
			continue ;
		want = split_tab(line);
		if (want == NULL)
		{
			report_fail(score, no, line, "(sekme ile ayrilmis iki alan)",
				"sekme yok, vaka kosulamadi");
			continue ;
		}
		run_case(score, no, line, want);
	}
	fclose(fp);
	return (1);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*path;
	t_score		score;

	path = "test/cases/parser.tsv";
	if (argc > 1)
		path = argv[1];
	score.passed = 0;
	score.failed = 0;
	printf("parser testleri (%s)\n", path);
	if (run_file(path, &score) == 0)
		return (1);
	printf("  %s%d gecti%s, %d patladi\n", GREEN, score.passed, OFF,
		score.failed);
	if (score.failed != 0)
		return (1);
	return (0);
}
