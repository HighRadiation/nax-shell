/*
** exec.c — boru hattini kurar ve calistirir.
**
** CIKIS KODU SEMANTIGI (bash olculerek dogrulandi):
**   normal cikis        cocugun kendi kodu
**   sinyalle olum       128 + sinyal numarasi
**   bulunamadi          127
**   calistirilamadi     126
**   yonlendirme hatasi  1
**   bos komut           0, hicbir sey calismaz
**   boru hatti          SON komutun kodu
**
** Son satir onemli: "false | true" 0, "true | false" 1 doner. Ama TUM
** cocuklar beklenmek zorunda, yoksa zombi kalir.
**
** COZUMLEME COCUKTA YAPILIR:
**   Her asama kendi komutunu cozer. Boru hattinda zorunlu, tek komutta da
**   ayni yolu kullanmak kod yolunu tekilestiriyor. Gozlenebilir davranis
**   ayni: bulunamayan komut yine 127 doner, mesaj yine kabugun stderr'ine
**   gider.
**
** EINTR DONGUSU:
**   Sinyaller SA_RESTART olmadan kuruldu (gerekcesi signal.c icinde), yani
**   onplanda bir komut kosarken Ctrl-C waitpid'i keser. Sarmazsak kabuk
**   cocugu beklemeyi birakir, zombi kalir ve cikis kodu uydurma olur.
**
** HENUZ YOK:
**   "<<" yonlendirmesi. Govdesini okumak icin okuma dongusunden ek satir
**   almak gerekiyor; bu, kabugun girdi yolunu degistiren ayri bir is.
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
void	ex_warn(const char *message)
{
	fflush(stdout);
	if (message == NULL)
		message = "bilinmeyen hata";
	fprintf(stderr, "%s: %s\n", NAX_NAME, message);
}

/* "ad: sebep" bicimindeki hata mesajini yazar. */
void	ex_warn_name(const char *name, const char *reason)
{
	fflush(stdout);
	fprintf(stderr, "%s: %s: %s\n", NAX_NAME, name, reason);
}

/*
** Alan listesini execve icin NULL ile biten diziye cevirir.
**
** Metinler KOPYALANMAZ, alan listesinden odunc alinir; liste bu dizinin
** omrunden uzun yasiyor. Serbest birakilirken yalnizca dizi birakilir.
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

/* Komutu cozer ve calistirir; bu islev donmez. */
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

/* Cocukta boru uclarini baglar ve fazlaligi kapatir. */
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
*/
static void	child_stage(const t_xcmd *xcmd, const t_stage *st)
{
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
	exec_or_die(xcmd->args);
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
		ex_warn(strerror(errno));
		return (1);
	}
	if (WIFSIGNALED(status))
		return (128 + WTERMSIG(status));
	return (WEXITSTATUS(status));
}

/* Bir asamayi genisletip catallar; pid'i dondurur, hata halinde -1. */
static pid_t	fork_stage(t_shell *sh, const t_cmd *cmd, const t_stage *st)
{
	t_exp_err	err;
	t_xcmd		xcmd;
	pid_t		pid;

	if (exp_cmd(cmd, sh, &xcmd, &err) == 0)
	{
		ex_warn(err.message);
		return (-1);
	}
	pid = fork();
	if (pid == 0)
		child_stage(&xcmd, st);
	if (pid < 0)
		ex_warn(strerror(errno));
	xcmd_free(&xcmd);
	return (pid);
}

/* Boru hattindaki komut sayisini verir. */
static size_t	stage_count(const t_cmd *cmds)
{
	size_t	n;

	n = 0;
	while (cmds != NULL)
	{
		n++;
		cmds = cmds->next;
	}
	return (n);
}

/*
** Tum asamalari borularla baglayarak catallar.
**
** Gercekten catallanan asama sayisini dondurur; boru acilamazsa yarida
** kesilir ama o ana kadar catallanmis cocuklar yine beklenir, yoksa zombi
** kalirlar.
*/
static size_t	fork_all(t_shell *sh, const t_cmd *cmds, pid_t *pids)
{
	t_stage	st;
	int		fds[2];
	size_t	i;

	st.in_fd = -1;
	i = 0;
	while (cmds != NULL)
	{
		st.out_fd = -1;
		st.spare_fd = -1;
		if (cmds->next != NULL && pipe(fds) < 0)
		{
			ex_warn(strerror(errno));
			break ;
		}
		if (cmds->next != NULL)
		{
			st.out_fd = fds[1];
			st.spare_fd = fds[0];
		}
		pids[i] = fork_stage(sh, cmds, &st);
		if (st.in_fd >= 0)
			close(st.in_fd);
		st.in_fd = -1;
		if (cmds->next != NULL)
		{
			close(fds[1]);
			st.in_fd = fds[0];
		}
		i++;
		cmds = cmds->next;
	}
	if (st.in_fd >= 0)
		close(st.in_fd);
	return (i);
}

/* Tum cocuklari bekler ve SON asamanin cikis kodunu dondurur. */
static int	wait_all(const pid_t *pids, size_t n)
{
	size_t	i;
	int		code;
	int		last;

	last = 1;
	i = 0;
	while (i < n)
	{
		if (pids[i] < 0)
			code = 1;
		else
			code = wait_child(pids[i]);
		if (i + 1 == n)
			last = code;
		i++;
	}
	return (last);
}

/* Bu asamada calistirilamayan yapiyi soyler; yoksa NULL doner. */
static const char	*unsupported(const t_cmd *cmds)
{
	const t_redir	*redir;

	while (cmds != NULL)
	{
		redir = cmds->redirs;
		while (redir != NULL)
		{
			if (redir->type == T_HEREDOC)
				return ("<< yonlendirmesi bu asamada calistirilmiyor");
			redir = redir->next;
		}
		cmds = cmds->next;
	}
	return (NULL);
}

/*
** Boru hattini calistirir ve kabugun son durumunu gunceller.
**
** Sondaki kesme kontrolu onemli: onplanda kosan cocuk Ctrl-C ile
** oldugunde sinyal kabuga da gelir ve bayrak set kalir. Temizlenmezse bir
** sonraki okumada readline satiri kullanici bir sey yazmadan iptal eder.
*/
void	ex_run(t_shell *sh, const t_cmd *cmds)
{
	const char	*reason;
	pid_t		*pids;
	size_t		n;

	if (cmds == NULL)
		return ;
	reason = unsupported(cmds);
	if (reason != NULL)
	{
		ex_warn(reason);
		sh->last_status = 1;
		return ;
	}
	pids = malloc(stage_count(cmds) * sizeof(*pids));
	if (pids == NULL)
	{
		ex_warn("bellek ayrilamadi");
		sh->last_status = 1;
		return ;
	}
	n = fork_all(sh, cmds, pids);
	sh->last_status = wait_all(pids, n);
	free(pids);
	if (sig_take_interrupt())
		write(STDOUT_FILENO, "\n", 1);
}
