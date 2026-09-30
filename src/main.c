/*
** main.c — giris noktasi ve okuma dongusu.
**
** NEDEN VAR:
**   Kabugun omru burada baslar ve biter: durumu kur, satirlari oku, isle,
**   sonra her seyi geri birak. Satirin nasil okundugu line.c'nin isi;
**   satirin ne anlama geldigi ise siniflandiriciya (classifier.c) gececek.
**
** BU ASAMADA:
**   Satir sozcuklere ayrilip ayristiriliyor ve CALISTIRILIYOR. Tek komut
**   gercekten kosuyor; boru hatti ve yonlendirme icin calistirici
**   anlasilir bir mesaj veriyor.
**
** CIKIS KODLARI:
**   Sozdizimi hatasi 2. Calistirma tarafindaki kodlar exec.c icinde
**   yazili. $? hepsini okuyor.
*/

#include "nax.h"
#include "exec.h"
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
** Hatayi kullaniciya bildirir ve verilen cikis kodunu ayarlar.
**
** NEDEN ONCE fflush:
**   stdout boruya yazarken blok tamponlu, stderr ise tamponsuz. Tamponu
**   bosaltmadan hata yazmak, "nax < betik > log 2>&1" gibi bir kullanimda
**   hatalarin ciktidan once gorunmesine yol acar; olculdu.
*/
static void	report_error(t_shell *sh, const char *message, int status)
{
	fflush(stdout);
	if (message == NULL)
		message = "bilinmeyen hata";
	fprintf(stderr, "%s: %s\n", NAX_NAME, message);
	sh->last_status = status;
}

/* Token listesinden boru hattini kurar ve calistiriciya verir. */
static void	run_tokens(t_shell *sh, const t_token *tokens)
{
	t_ast_err	err;
	t_cmd		*cmds;

	cmds = ast_build(tokens, &err);
	if (cmds == NULL)
	{
		if (err.message != NULL)
			report_error(sh, err.message, 2);
		return ;
	}
	ex_run(sh, cmds);
	ast_free(cmds);
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
		report_error(sh, err.message, 2);
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
