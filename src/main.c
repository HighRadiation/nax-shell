/*
** main.c - giris noktasi ve okuma dongusu.
**
** NEDEN VAR:
**   Kabugun omru burada baslar ve biter: durumu kur, satirlari oku, isle,
**   sonra her seyi geri birak. Satirin nasil okundugu line.c'nin isi;
**   satirin ne anlama geldigi ise siniflandiriciya (classifier.c) gececek.
**
** BU ASAMADA:
**   Satir once SINIFLANDIRILIYOR: kabuk komutu mu, dogal dil mi. Kabuk
**   komutuysa ayristirilip calistiriliyor; dogal dilse simdilik bir bilgi
**   satiri basiliyor, cunku yardimci surec henuz yok.
**
**   Bos satir ve denetim karakteri filtresi de artik siniflandiricinin
**   isi; main.c'deki eski is_blank kontrolu oraya tasindi.
**
**   "exit" artik gecici bir ozel durum DEGIL, gercek bir yerlesik. Onceki
**   surumde satir daha ayristirilmadan yakalaniyordu; o kestirme
**   kaldirildi, cunku "exit 7" ve "exit abc" gibi haller ancak normal
**   hattan gecerek dogru davranabiliyor.
**
** CIKIS KODLARI:
**   Sozdizimi hatasi 2. Calistirma tarafindaki kodlar exec.c icinde
**   yazili. $? hepsini okuyor.
*/

#include "nax.h"
#include "ai.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/*
** AI yolunu simdilik bir bilgi satiriyla karsilar.
**
** Siniflandirici kararini veriyor ama yardimci surec henuz yok. Mesaj
** kararin GORUNUR olmasi icin: hangi satirin niyet sayildigini ve hangi
** vetonun tetiklendigini gozle dogrulamak, korpus testinin yanindaki
** ikinci gozlem yolu.
*/
static void	report_route(t_shell *sh, const t_decision *d)
{
	fflush(stdout);
	if (d->route == ROUTE_EXPLAIN)
		fprintf(stderr, "%s: son hata aciklamasi henuz bagli degil\n",
			NAX_NAME);
	else
		fprintf(stderr, "%s: niyet (veto %#x) henuz bagli degil: %s\n",
			NAX_NAME, d->vetoes, d->text);
	sh->last_status = 1;
}

/* Tek bir girdi satirini isler: siniflandirir, sonra yoluna gonderir. */
static void	handle_line(t_shell *sh, const char *line)
{
	t_lex_err	err;
	t_token		*tokens;
	t_decision	d;

	d = cls_classify(line, &tokens, &err);
	if (d.route == ROUTE_SHELL)
		run_tokens(sh, tokens);
	else if (d.route == ROUTE_SYNTAX_ERR)
		report_error(sh, err.message, 2);
	else if (d.route != ROUTE_EMPTY)
		report_route(sh, &d);
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
