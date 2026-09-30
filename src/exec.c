/*
** exec.c — komutu gercekten calistirir.
**
** BU ASAMADA:
**   Tek komut calisiyor: cozumleme, catallama, execve, bekleme ve gercek
**   cikis kodlari. Boru hatti ve yonlendirme HENUZ calistirilmiyor;
**   ikisinde de anlasilir bir mesaj verilip 1 donuyor. Ayristirici ve
**   genisletme ikisini de dogru isliyor, eksik olan yalnizca burasi.
**
** CIKIS KODU SEMANTIGI (bash olculerek dogrulandi):
**   normal cikis      cocugun kendi kodu
**   sinyalle olum     128 + sinyal numarasi
**   bulunamadi        127
**   calistirilamadi   126
**   bos komut         0, hicbir sey calismaz
**
** Bos komut hali gercek bir durum: "$YOKBOYLE" tek basina yazildiginda
** genisletme hic alan uretmez, yani calistirilacak bir sey yoktur. Bash
** de bu durumda 0 doner.
**
** EINTR DONGUSU:
**   Sinyaller SA_RESTART olmadan kuruldu (gerekcesi signal.c icinde), yani
**   on planda bir komut kosarken Ctrl-C waitpid'i keser. Sarmazsak kabuk
**   cocugu beklemeyi birakir, zombi kalir ve cikis kodu uydurma olur. Bu
**   kosul docs/FINDINGS.md icinde bekliyordu ve burada odendi.
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

/* Hata mesajini stdout tamponunu bosaltarak stderr'e yazar. */
static void	warn(const char *message)
{
	fflush(stdout);
	fprintf(stderr, "%s: %s\n", NAX_NAME, message);
}

/* "ad: sebep" bicimindeki hata mesajini yazar. */
static void	warn_name(const char *name, const char *reason)
{
	fflush(stdout);
	fprintf(stderr, "%s: %s: %s\n", NAX_NAME, name, reason);
}

/*
** Alan listesini execve icin NULL ile biten diziye cevirir.
**
** Metinler KOPYALANMAZ, alan listesinden odunc alinir; liste bu dizinin
** omrunden uzun yasiyor. Bu yuzden serbest birakilirken yalnizca dizinin
** kendisi birakilir, icindeki isaretciler degil.
*/
static char	**build_argv(const t_field *fields)
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

/* Cocuk surecte komutu calistirir; bu islev donmez. */
static void	child_exec(const char *path, char **argv)
{
	sig_reset_child();
	execve(path, argv, environ);
	warn_name(argv[0], strerror(errno));
	_exit(126);
}

/* Cocugu bekler ve cikis kodunu dondurur; EINTR dongusu zorunlu. */
static int	wait_child(pid_t pid)
{
	int	status;
	int	got;

	got = waitpid(pid, &status, 0);
	while (got < 0 && errno == EINTR)
		got = waitpid(pid, &status, 0);
	if (got < 0)
	{
		warn(strerror(errno));
		return (1);
	}
	if (WIFSIGNALED(status))
		return (128 + WTERMSIG(status));
	return (WEXITSTATUS(status));
}

/* Cozulmus komutu catallayip calistirir ve cikis kodunu dondurur. */
static int	spawn(const char *path, char **argv)
{
	pid_t	pid;

	pid = fork();
	if (pid < 0)
	{
		warn(strerror(errno));
		return (1);
	}
	if (pid == 0)
		child_exec(path, argv);
	return (wait_child(pid));
}

/* Genisletilmis komutu cozer ve calistirir; cikis kodunu dondurur. */
static int	run_xcmd(const t_xcmd *xcmd)
{
	t_resolve	status;
	char		*path;
	char		**argv;
	int			code;

	if (xcmd->args == NULL)
		return (0);
	status = path_resolve(xcmd->args->text, &path);
	if (status != RES_OK)
	{
		warn_name(xcmd->args->text, path_reason(status));
		return (path_code(status));
	}
	argv = build_argv(xcmd->args);
	if (argv == NULL)
	{
		free(path);
		warn("bellek ayrilamadi");
		return (1);
	}
	code = spawn(path, argv);
	free(argv);
	free(path);
	return (code);
}

/* Bu asamada calistirilamayan yapiyi soyler; yoksa NULL doner. */
static const char	*unsupported(const t_cmd *cmds)
{
	if (cmds->next != NULL)
		return ("boru hatti bu asamada calistirilmiyor");
	if (cmds->redirs != NULL)
		return ("yonlendirme bu asamada calistirilmiyor");
	return (NULL);
}

/*
** Boru hattini calistirir ve kabugun son durumunu gunceller.
**
** Sondaki kesme kontrolu onemli: on planda kosan cocuk Ctrl-C ile
** oldugunde sinyal kabuga da gelir ve bayrak set kalir. Temizlenmezse bir
** sonraki okumada readline satiri kullanici bir sey yazmadan iptal eder.
*/
void	ex_run(t_shell *sh, const t_cmd *cmds)
{
	t_exp_err	err;
	t_xcmd		xcmd;
	const char	*reason;

	if (cmds == NULL)
		return ;
	reason = unsupported(cmds);
	if (reason != NULL)
	{
		warn(reason);
		sh->last_status = 1;
		return ;
	}
	if (exp_cmd(cmds, sh, &xcmd, &err) == 0)
	{
		warn(err.message);
		sh->last_status = 1;
		return ;
	}
	sh->last_status = run_xcmd(&xcmd);
	xcmd_free(&xcmd);
	if (sig_take_interrupt())
		write(STDOUT_FILENO, "\n", 1);
}
