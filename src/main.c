/*
** main.c — giris noktasi ve okuma dongusu.
**
** NEDEN VAR:
**   Kabugun omru burada baslar ve biter: durumu kur, satirlari oku, isle,
**   sonra her seyi geri birak. Satirin nasil okundugu line.c'nin isi;
**   satirin ne anlama geldigi ise siniflandiriciya (classifier.c) gececek.
**
** BU ASAMADA:
**   Satir sozcuklere ayrilip ayristiriliyor, sonra uretilen boru hattinin
**   kanonik metni basiliyor. Calistirici geldiginde bu basim onun
**   cagrisiyla degisecek; agacin dogru kuruldugunu gozle gormek ve hattin
**   gercek ikilide de test edilmesi icin simdi burada.
*/

#include "nax.h"
#include "parse.h"
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

/*
** Sozdizimi hatasini kullaniciya bildirir ve cikis kodunu 2 yapar.
**
** NEDEN ONCE fflush:
**   stdout boruya yazarken blok tamponlu, stderr ise tamponsuz. Tamponu
**   bosaltmadan hata yazmak, "nax < betik > log 2>&1" gibi bir kullanimda
**   hatalarin ciktidan once gorunmesine yol acar; olculdu.
*/
static void	report_syntax_error(t_shell *sh, const char *message)
{
	fflush(stdout);
	fprintf(stderr, "%s: %s\n", NAX_NAME, message);
	sh->last_status = 2;
}

/* Token listesinden boru hattini kurar ve kanonik metnini basar. */
static void	run_tokens(t_shell *sh, const t_token *tokens)
{
	t_ast_err	err;
	t_cmd		*cmds;
	char		*shape;

	cmds = ast_build(tokens, &err);
	if (cmds == NULL)
	{
		if (err.message != NULL)
			report_syntax_error(sh, err.message);
		return ;
	}
	shape = ast_dump(cmds);
	if (shape != NULL)
		printf("%s\n", shape);
	free(shape);
	ast_free(cmds);
	sh->last_status = 0;
}

/* Tek bir girdi satirini isler: ayirir, ayristirir, sonucu basar. */
static void	handle_line(t_shell *sh, const char *line)
{
	t_lex_err	err;
	t_token		*tokens;

	if (is_blank(line))
		return ;
	if (is_exit_request(line))
	{
		sh->exiting = 1;
		return ;
	}
	tokens = lex_split(line, &err);
	if (tokens == NULL && err.message != NULL)
	{
		report_syntax_error(sh, err.message);
		return ;
	}
	run_tokens(sh, tokens);
	lex_free(tokens);
}

/* Kabuk durumunu ilk degerlerine kurar ve gecmisi yukler. */
static void	shell_init(t_shell *sh)
{
	sh->last_status = 0;
	sh->interactive = isatty(STDIN_FILENO);
	sh->exiting = 0;
	if (sh->interactive)
		sig_setup_interactive();
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

/*
** Okuma dongusu: satir al, isle, birak. EOF ya da "exit" ile biter.
**
** Kesme kontrolu satir kontrolunden ONCE yapilir: Ctrl-C ile iptal edilen
** satir bos bir dize olarak doner, gercek dosya sonu ise NULL doner. Ikisi
** yalnizca kesme bayragiyla ayrilir.
*/
static void	loop(t_shell *sh)
{
	char	*line;

	while (sh->exiting == 0)
	{
		line = ln_read(sh);
		if (sig_take_interrupt())
		{
			free(line);
			sh->last_status = 130;
			continue ;
		}
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
