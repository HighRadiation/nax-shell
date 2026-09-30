/*
** main.c — giris noktasi ve okuma dongusu.
**
** NEDEN VAR:
**   Kabugun omru burada baslar ve biter: durumu kur, satirlari oku, isle,
**   sonra her seyi geri birak. Satirin nasil okundugu line.c'nin isi;
**   satirin ne anlama geldigi ise siniflandiriciya (classifier.c) gececek.
**
** BU ASAMADA:
**   Satir henuz yorumlanmiyor, yalnizca geri yaziliyor. "exit" ve Ctrl-D
**   kabuktan cikarir. Lexer, ayristirici ve calistirici sonraki asamada
**   devreye girecek ve handle_line govdesi onlara devredilecek.
*/

#include "nax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Bastaki bosluklari atlar; ilk bosluk olmayan karakteri gosterir. */
static const char	*skip_blank(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}

/* Satirin tamamen bos (yalnizca bosluk) olup olmadigini soyler. */
static int	is_blank(const char *line)
{
	return (*skip_blank(line) == '\0');
}

/* Satirin tek basina "exit" komutu olup olmadigini soyler. */
static int	is_exit_request(const char *line)
{
	const char	*p;

	p = skip_blank(line);
	if (strncmp(p, "exit", 4) != 0)
		return (0);
	p = skip_blank(p + 4);
	return (*p == '\0');
}

/* Tek bir girdi satirini isler; bu asamada yalnizca geri yazar. */
static void	handle_line(t_shell *sh, const char *line)
{
	if (is_blank(line))
		return ;
	if (is_exit_request(line))
	{
		sh->exiting = 1;
		return ;
	}
	printf("%s\n", line);
	sh->last_status = 0;
}

/* Kabuk durumunu ilk degerlerine kurar ve gecmisi yukler. */
static void	shell_init(t_shell *sh)
{
	sh->last_status = 0;
	sh->interactive = isatty(STDIN_FILENO);
	sh->exiting = 0;
	ln_setup();
	sh->hist_path = ln_hist_path();
	ln_hist_load(sh->hist_path);
}

/* Gecmisi diske yazar ve ayrilan tum bellegi birakir. */
static void	shell_free(t_shell *sh)
{
	ln_hist_save(sh->hist_path);
	free(sh->hist_path);
	sh->hist_path = NULL;
}

/* Okuma dongusu: satir al, isle, birak. EOF ya da "exit" ile biter. */
static void	loop(t_shell *sh)
{
	char	*line;

	while (sh->exiting == 0)
	{
		line = ln_read(sh);
		if (line == NULL)
		{
			if (sh->interactive)
				printf("exit\n");
			break ;
		}
		handle_line(sh, line);
		free(line);
	}
}

/* Giris noktasi; kabugun cikis kodu son isin durumudur. */
int	main(void)
{
	t_shell	sh;

	shell_init(&sh);
	loop(&sh);
	shell_free(&sh);
	return (sh.last_status);
}
