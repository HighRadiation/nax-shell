/*
** stage.c - catallanmis bir asamanin COCUK tarafinda yaptigi is.
**
** NEDEN AYRI MODUL:
**   exec.c ana surecin isini yapiyor: kim catallanacak, kim beklenecek,
**   hangi kod dondurulecek. Burasi catallandiktan SONRA cocugun yaptigi
**   is: boru uclarini bagla, yonlendirmeleri uygula, yerlesik ya da
**   harici komutu calistir. Ikisi farkli baglamda kosuyor ve farkli
**   kurallara tabi - cocukta hata halinde temizlik yapilmaz, _exit edilir.
**
** COCUKTA NEDEN _exit VE exit DEGIL:
**   exit atexit islevlerini kosar ve stdio tamponlarini bosaltir; cocuk
**   ana surecten devraldigi tamponu ikinci kez basardi. _exit bunlari
**   atlar. Tampon bilincli olarak bosaltilmasi gereken yerde elle
**   fflush cagriliyor.
*/

#include "nax.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

extern char	**environ;

/*
** Alan listesini NULL ile biten diziye cevirir.
**
** Metinler KOPYALANMAZ, alan listesinden odunc alinir; liste bu dizinin
** omrunden uzun yasiyor. Serbest birakilirken yalnizca dizi birakilir.
*/
char	**build_argv(const t_field *fields)
{
	const t_field	*walk;
	char			**argv;
	size_t			count;

	count = 0;
	walk = fields;
	while (walk != NULL)
	{
		count++;
		walk = walk->next;
	}
	argv = malloc((count + 1) * sizeof(*argv));
	if (argv == NULL)
		return (NULL);
	count = 0;
	while (fields != NULL)
	{
		argv[count] = fields->text;
		count++;
		fields = fields->next;
	}
	argv[count] = NULL;
	return (argv);
}

/* Cocugu bekler ve cikis kodunu dondurur; EINTR dongusu zorunlu. */
int	wait_child(pid_t pid)
{
	int	status;
	int	got;

	got = waitpid(pid, &status, 0);
	while (got < 0 && errno == EINTR)
		got = waitpid(pid, &status, 0);
	if (got < 0)
	{
		ex_warn(strerror(errno));
		return (1);
	}
	if (WIFSIGNALED(status))
		return (128 + WTERMSIG(status));
	return (WEXITSTATUS(status));
}

/* Harici komutu cozer ve calistirir; bu islev donmez. */
static void	exec_or_die(const t_field *args)
{
	t_resolve	status;
	char		*path;
	char		**argv;

	status = path_resolve(args->text, &path);
	if (status != RES_OK)
	{
		ex_warn_name(args->text, path_reason(status));
		_exit(path_code(status));
	}
	argv = build_argv(args);
	if (argv == NULL)
	{
		ex_warn("bellek ayrilamadi");
		_exit(1);
	}
	execve(path, argv, environ);
	ex_warn_name(argv[0], strerror(errno));
	_exit(126);
}

/* Yerlesigi cocukta kosar ve cikis kodu ile biter; bu islev donmez. */
static void	builtin_or_die(t_shell *sh, t_builtin_fn fn, const t_field *args)
{
	char	**argv;
	int		code;

	argv = build_argv(args);
	if (argv == NULL)
	{
		ex_warn("bellek ayrilamadi");
		_exit(1);
	}
	code = fn(sh, argv);
	free(argv);
	fflush(stdout);
	_exit(code);
}

/*
** Cocukta boru uclarini baglar ve fazlaligi kapatir.
**
** spare_fd ilk isi olarak kapatilir: cocuk kendi cikis borusunun OKUMA
** ucunu da devralir ve kapatilmazsa o tanimlayici execve ile calistirilan
** programa sizar. Bu bir asilma sebebi degil, sizma sebebi; ayrimi ve
** asilmanin gercek sebebi exec.h icinde yazili.
*/
static int	wire_pipes(const t_stage *st)
{
	if (st->spare_fd >= 0)
		close(st->spare_fd);
	if (st->in_fd >= 0)
	{
		if (dup2(st->in_fd, STDIN_FILENO) < 0)
			return (0);
		close(st->in_fd);
	}
	if (st->out_fd >= 0)
	{
		if (dup2(st->out_fd, STDOUT_FILENO) < 0)
			return (0);
		close(st->out_fd);
	}
	return (1);
}

/*
** Cocuk surecte asamayi kurar ve calistirir; bu islev donmez.
**
** Yonlendirmeler borulardan SONRA uygulanir, cunku catisma halinde
** yonlendirme kazanmak zorunda: "ls > f | wc" ciktisini dosyaya yazar,
** wc'ye hicbir sey gitmez.
**
** Yerlesik kontrolu argv[0] GENISLETILDIKTEN sonra yapilir, boylece
** CMD=echo iken "$CMD merhaba" de calisir.
*/
void	child_stage(t_shell *sh, const t_xcmd *xcmd, const t_stage *st)
{
	t_builtin_fn	fn;

	sig_reset_child();
	if (wire_pipes(st) == 0)
	{
		ex_warn(strerror(errno));
		_exit(1);
	}
	if (redir_apply(xcmd->redirs) == 0)
		_exit(1);
	if (xcmd->args == NULL)
		_exit(0);
	fn = bi_lookup(xcmd->args->text);
	if (fn != NULL)
		builtin_or_die(sh, fn, xcmd->args);
	exec_or_die(xcmd->args);
}
