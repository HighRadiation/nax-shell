/*
** line.c — satir okuma, prompt ve gecmis.
**
** NEDEN AYRI MODUL:
**   readline'in tum tuhafliklari (renk kaclarinin \001..\002 ile sarilmasi,
**   gecmis dosyasi, ileride oneriyi duzenleme tamponuna on-yukleme) tek
**   yerde kalsin. main.c "bana bir satir ver" demekten baska sey bilmez.
**
** RENK TUZAGI:
**   readline prompt genisligini sayarken basilmayan baytlari saymaz, ama
**   bunu bilmesi icin onlarin \001 ile \002 arasina alinmasi gerekir.
**   Sarilmazsa uzun satirlarda imlec kayar. CLR_* makrolari bu yuzden
**   kaclari zaten sarili tutar.
*/

#include "nax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>

#define HIST_FILE ".nax_history"
#define CLR_NAME "\001\033[1;36m\002"
#define CLR_OFF "\001\033[0m\002"
#define PROMPT_EXTRA 64

/* HOME ile baslayan yolu ~ ile kisaltir; cagiran serbest birakir. */
static char	*shorten_path(const char *path)
{
	const char	*home;
	size_t		len;
	char		*out;

	home = getenv("HOME");
	if (home == NULL || *home == '\0')
		return (strdup(path));
	len = strlen(home);
	if (strncmp(path, home, len) != 0)
		return (strdup(path));
	if (path[len] != '\0' && path[len] != '/')
		return (strdup(path));
	out = malloc(strlen(path) - len + 2);
	if (out == NULL)
		return (NULL);
	out[0] = '~';
	memcpy(out + 1, path + len, strlen(path) - len + 1);
	return (out);
}

/* Calisma dizinini kisaltilmis haliyle dondurur; cagiran serbest birakir. */
static char	*current_dir(void)
{
	char	buf[PATH_MAX];

	if (getcwd(buf, sizeof(buf)) == NULL)
		return (strdup("?"));
	return (shorten_path(buf));
}

/* Gosterilecek prompt metnini uretir; cagiran serbest birakir. */
static char	*build_prompt(void)
{
	char	*dir;
	char	*prompt;
	size_t	size;

	dir = current_dir();
	if (dir == NULL)
		return (NULL);
	size = strlen(dir) + strlen(CLR_NAME) + strlen(CLR_OFF) + PROMPT_EXTRA;
	prompt = malloc(size);
	if (prompt == NULL)
	{
		free(dir);
		return (NULL);
	}
	snprintf(prompt, size, "%s%s%s %s $ ", CLR_NAME, NAX_NAME, CLR_OFF, dir);
	free(dir);
	return (prompt);
}

/* readline'i bu kabuk adina tanitir; ~/.inputrc kurallari buna gore isler. */
void	ln_setup(void)
{
	rl_readline_name = NAX_NAME;
}

/* Gecmis dosyasinin tam yolunu uretir; HOME yoksa NULL doner. */
char	*ln_hist_path(void)
{
	const char	*home;
	char		*path;
	size_t		size;

	home = getenv("HOME");
	if (home == NULL || *home == '\0')
		return (NULL);
	size = strlen(home) + strlen(HIST_FILE) + 2;
	path = malloc(size);
	if (path == NULL)
		return (NULL);
	snprintf(path, size, "%s/%s", home, HIST_FILE);
	return (path);
}

/* Varsa gecmis dosyasini readline'a yukler. */
void	ln_hist_load(const char *path)
{
	if (path == NULL)
		return ;
	read_history(path);
}

/* Gecmisi diske yazar; yol uretilememisse hicbir sey yapmaz. */
void	ln_hist_save(const char *path)
{
	if (path == NULL)
		return ;
	write_history(path);
}

/*
** Etkilesimsiz girdide bir satir okur; sondaki yeni satiri atar.
**
** NEDEN readline DEGIL:
**   readline stdin bir terminal olmadiginda okudugu satiri stdout'a
**   yankilar. Bu, betik ve test ciktisini ikiye katlar. Duzenleme
**   yetenegi de zaten yalnizca terminalde anlamli oldugu icin
**   etkilesimsiz yol dogrudan getline kullanir.
*/
static char	*read_plain(void)
{
	char	*line;
	size_t	cap;
	ssize_t	len;

	line = NULL;
	cap = 0;
	len = getline(&line, &cap, stdin);
	if (len < 0)
	{
		free(line);
		return (NULL);
	}
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';
	return (line);
}

/* Terminalde prompt basip bir satir okur ve bos degilse gecmise ekler. */
static char	*read_interactive(void)
{
	char	*prompt;
	char	*line;

	prompt = build_prompt();
	if (prompt != NULL)
		line = readline(prompt);
	else
		line = readline("");
	free(prompt);
	if (line != NULL && *line != '\0')
		add_history(line);
	return (line);
}

/* Bir satir okur; cagiran serbest birakir. EOF'ta NULL doner. */
char	*ln_read(const t_shell *sh)
{
	if (sh->interactive == 0)
		return (read_plain());
	return (read_interactive());
}
