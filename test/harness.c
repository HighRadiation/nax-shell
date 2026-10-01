/*
** harness.c - tablo tabanli testlerin ortak iskelesi.
**
** VAKA DOSYASI BICIMI:
**   girdi <SEKME> beklenen
**   "#" ile baslayan ve bos satirlar atlanir.
**
** SEKMESIZ SATIR SESSIZCE ATLANMAZ:
**   Vaka olarak patlar. Sessiz atlama yesil bir suite gosterir ama hicbir
**   sey olcmez; bu tuzak gercekten yasandi - vaka dosyasi bir kez gercek
**   sekme yerine "\t" metniyle uretilmisti ve tum vakalar sessizce
**   atlanmisti.
**
** KACISLAR:
**   Her iki alanda \t ve \\ cozulur. \n bilincli olarak DESTEKLENMEZ:
**   ters egik cizgi testlerinde "\n" dizisinin harfi harfine kalmasi
**   gerekiyor.
*/

#include "test.h"
#include <stdio.h>
#include <string.h>

#define LINE_MAX_LEN 4096

/*
** Metin icindeki \t, \e ve \\ kacislarini yerinde cozer.
**
** \e SONRADAN EKLENDI: siniflandiricinin denetim karakteri filtresini
** sinayan vaka TSV'ye baska turlu yazilamiyordu. Sekme ayirici, satirsonu
** vaka sonu; ESC ise terminalin sizdirdigi CSI dizilerinin ilk bayti.
*/
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
		else if (*s == '\\' && s[1] == 'e')
		{
			*out = 27;
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

/*
** Metni ciktinin sonuna ekler; tasarsa sessizce kesilir.
**
** NEDEN ISKELEDE: birden fazla test dosyasi beklenen degeri parca parca
** kuruyor. Her birinde ayni ekleme islevini tutmak, modulerlik kuralinin
** yasakladigi sey - ayni is iki yerde.
*/
void	test_append(char *out, size_t cap, const char *text)
{
	size_t	len;

	len = strlen(out);
	if (len + 1 >= cap)
		return ;
	snprintf(out + len, cap - len, "%s", text);
}

/* Basarisiz vakayi beklenen ve gelen degerlerle birlikte basar. */
void	report_fail(t_score *score, int no, const char *input,
		const char *want, const char *got)
{
	score->failed++;
	printf("  %spatladi%s satir %d\n", TEST_RED, TEST_OFF, no);
	printf("    girdi    : %s\n", input);
	printf("    beklenen : %s\n", want);
	printf("    gelen    : %s\n", got);
}

/* Tek bir satiri vakaya cevirir ve verilen isleve verir. */
static void	run_line(t_score *score, int no, char *line, t_case_fn fn)
{
	char	*want;

	want = split_tab(line);
	if (want == NULL)
	{
		report_fail(score, no, line, "(sekme ile ayrilmis iki alan)",
			"sekme yok, vaka kosulamadi");
		return ;
	}
	unescape(line);
	unescape(want);
	fn(score, no, line, want);
}

/* Vaka dosyasini kosar ve surecin cikis kodunu dondurur. */
int	harness_run(const char *path, const char *title, t_case_fn fn)
{
	FILE	*fp;
	char	line[LINE_MAX_LEN];
	t_score	score;
	int		no;

	score.passed = 0;
	score.failed = 0;
	printf("%s (%s)\n", title, path);
	fp = fopen(path, "r");
	if (fp == NULL)
	{
		printf("  %sHATA%s: %s acilamadi\n", TEST_RED, TEST_OFF, path);
		return (1);
	}
	no = 0;
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		no++;
		chomp(line);
		if (line[0] != '#' && line[0] != '\0')
			run_line(&score, no, line, fn);
	}
	fclose(fp);
	printf("  %s%d gecti%s, %d patladi\n", TEST_GREEN, score.passed,
		TEST_OFF, score.failed);
	if (score.failed != 0)
		return (1);
	return (0);
}
