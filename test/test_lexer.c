/*
** test_lexer.c — sozcuk ayirici icin tablo tabanli testler.
**
** NEDEN TABLO:
**   Vaka eklemek yeniden derleme gerektirmesin. Vakalar
**   test/cases/lexer.tsv icinde durur: girdi, sekme, beklenen cikti.
**   Yeni bir kenar durum bulundugunda oraya bir satir eklenir.
**
** BEKLENEN CIKTI BICIMI:
**   Basarili ayirma icin lex_dump'in urettigi kanonik metin.
**   Hata beklenen vakalarda beklenen alan "!" ile baslar ve arkasindan
**   hata mesaji gelir.
**
** KACISLAR:
**   Sekme dosyanin alan ayiricisi oldugu icin her iki alanda \t ve \\
**   kacislari cozulur. \n bilincli olarak DESTEKLENMEZ: ters egik cizgi
**   testlerinde "\n" dizisinin harfi harfine kalmasi gerekiyor.
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

/* Sondaki yeni satir karakterini atar. */
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
		const char *want)
{
	t_lex_err	err;
	t_token		*tokens;

	tokens = lex_split(input, &err);
	if (tokens == NULL && err.message != NULL
		&& strcmp(err.message, want + 1) == 0)
	{
		score->passed++;
		return ;
	}
	if (tokens != NULL)
	{
		report_fail(score, no, input, want, "(hata beklendi, ayirma basardi)");
		lex_free(tokens);
		return ;
	}
	report_fail(score, no, input, want, err.message);
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
	unescape(input);
	unescape(want);
	if (want[0] == '!')
		check_error(score, no, input, want);
	else
		check_dump(score, no, input, want);
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

	path = "test/cases/lexer.tsv";
	if (argc > 1)
		path = argv[1];
	score.passed = 0;
	score.failed = 0;
	printf("lexer testleri (%s)\n", path);
	if (run_file(path, &score) == 0)
		return (1);
	printf("  %s%d gecti%s, %d patladi\n", GREEN, score.passed, OFF,
		score.failed);
	if (score.failed != 0)
		return (1);
	return (0);
}
