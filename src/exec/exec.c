/*
** exec.c - ANA surecin tarafi: kim catallanir, kim beklenir, hangi kod doner.
**
** Catallandiktan sonra cocugun yaptigi is stage.c icinde.
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
** TEK BASINA YERLESIK ANA SURECTE KOSAR:
**   cd, export, unset ve exit kabugun durumunu degistiriyor. Cocukta
**   kosarlarsa degisiklik cocukla birlikte yok olur ve "cd /tmp" hicbir
**   ise yaramaz. Boru hattinin ICINDEKI yerlesik ise cocukta kosar; bash
**   de boyle davraniyor, "echo x | cd /tmp" kabugun dizinini
**   degistirmiyor (olculdu).
**
**   Bunun bedeli: yonlendirme ana surecte 0 ve 1'i degistirdigi icin
**   once kaydedilip sonra geri yuklenmek zorunda. Cocukta bu sorun yoktu,
**   cunku cocuk zaten yok oluyordu.
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

/* Yerlesik hatasini yazar; arg NULL ise yalnizca sebep yazilir. */
void	ex_warn_bi(const char *builtin, const char *arg, const char *reason)
{
	fflush(stdout);
	if (arg == NULL)
		fprintf(stderr, "%s: %s: %s\n", NAX_NAME, builtin, reason);
	else
		fprintf(stderr, "%s: %s: %s: %s\n", NAX_NAME, builtin, arg, reason);
}

/* Ana surecte yerlesik kosmadan once 0 ve 1'i kaydeder. */
static int	fd_save(int saved[2])
{
	saved[0] = dup(STDIN_FILENO);
	saved[1] = dup(STDOUT_FILENO);
	return (saved[0] >= 0 && saved[1] >= 0);
}

/* Kaydedilen tanimlayicilari geri yukler. */
static void	fd_restore(int saved[2])
{
	if (saved[0] >= 0)
	{
		dup2(saved[0], STDIN_FILENO);
		close(saved[0]);
	}
	if (saved[1] >= 0)
	{
		dup2(saved[1], STDOUT_FILENO);
		close(saved[1]);
	}
}

/*
** Tek basina bir yerlesigi ana surecte kosar.
**
** Geri yuklemeden ONCE fflush zorunlu: yerlesigin tamponda bekleyen
** ciktisi bosaltilmazsa, tanimlayicilar geri yuklendikten sonra yanlis
** yere yazilir. Yani "pwd > dosya" ciktisini terminale basardi.
*/
static int	run_lone_builtin(t_shell *sh, t_builtin_fn fn, const t_xcmd *xcmd)
{
	int		saved[2];
	char	**argv;
	int		code;

	if (fd_save(saved) == 0)
	{
		ex_warn(strerror(errno));
		fd_restore(saved);
		return (1);
	}
	code = 1;
	if (redir_apply(xcmd->redirs) != 0)
	{
		argv = build_argv(xcmd->args);
		if (argv == NULL)
			ex_warn("bellek ayrilamadi");
		else
		{
			code = fn(sh, argv);
			free(argv);
		}
	}
	fflush(stdout);
	fd_restore(saved);
	return (code);
}

/* Tek komutu catallar, bekler ve cikis kodunu dondurur. */
static int	fork_and_wait(t_shell *sh, const t_xcmd *xcmd)
{
	t_stage	st;
	pid_t	pid;

	st.in_fd = -1;
	st.out_fd = -1;
	st.spare_fd = -1;
	pid = fork();
	if (pid == 0)
		child_stage(sh, xcmd, &st);
	if (pid < 0)
	{
		ex_warn(strerror(errno));
		return (1);
	}
	return (wait_child(pid));
}

/*
** Borusuz tek komutu calistirir.
**
** Yerlesik olup olmadigina argv[0] GENISLETILDIKTEN sonra bakilir,
** boylece CMD=cd iken "$CMD /tmp" de calisir; bash da boyle davraniyor.
*/
static int	run_one(t_shell *sh, const t_cmd *cmd)
{
	t_exp_err		err;
	t_xcmd			xcmd;
	t_builtin_fn	fn;
	int				code;

	if (exp_cmd(cmd, sh, &xcmd, &err) == 0)
	{
		ex_warn(err.message);
		return (1);
	}
	fn = NULL;
	if (xcmd.args != NULL)
		fn = bi_lookup(xcmd.args->text);
	if (fn != NULL)
		code = run_lone_builtin(sh, fn, &xcmd);
	else
		code = fork_and_wait(sh, &xcmd);
	xcmd_free(&xcmd);
	return (code);
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
		child_stage(sh, &xcmd, st);
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
**
** Ana surecin fds[1]'i kapatmasi ZORUNLU: boru EOF'u yazma uclari
** kapandiginda gorulur, kapatilmazsa okuyan asama sonsuza kadar bekler.
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

/* Cok asamali boru hattini kurar, kosar ve son kodu dondurur. */
static int	run_pipeline(t_shell *sh, const t_cmd *cmds)
{
	pid_t	*pids;
	size_t	n;
	int		code;

	pids = malloc(stage_count(cmds) * sizeof(*pids));
	if (pids == NULL)
	{
		ex_warn("bellek ayrilamadi");
		return (1);
	}
	n = fork_all(sh, cmds, pids);
	code = wait_all(pids, n);
	free(pids);
	return (code);
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

	if (cmds == NULL)
		return ;
	reason = unsupported(cmds);
	if (reason != NULL)
	{
		ex_warn(reason);
		sh->last_status = 1;
		return ;
	}
	if (cmds->next == NULL)
		sh->last_status = run_one(sh, cmds);
	else
		sh->last_status = run_pipeline(sh, cmds);
	if (sig_take_interrupt())
		write(STDOUT_FILENO, "\n", 1);
}
