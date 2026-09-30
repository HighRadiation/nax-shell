/*
** builtin_cd.c - dizin degistirme.
**
** NEDEN AYRI DOSYA:
**   cd tek bir yerlesik ama en cok yan etkisi olan: dizin degistirir, PWD
**   ve OLDPWD degiskenlerini gunceller, "-" bicimini tanir ve uc ayri
**   hata halini ayirt eder. Digerleriyle ayni dosyada tutmak ikisini de
**   kalabaliklastiriyordu.
**
** PWD VE OLDPWD NEDEN GUNCELLENIR:
**   Prompt getcwd kullaniyor, yani gorunus icin gerekli degil. Ama
**   "echo $PWD" genisletmesi ortamdan okuyor; guncellenmezse eski dizini
**   gosterir. Bash ikisini de guncelliyor (olculdu).
**
** HATA HALLERI (bash olculerek dogrulandi):
**   olmayan dizin  1, "cd: ad: No such file or directory"
**   dizin olmayan  1, "cd: ad: Not a directory"
**   fazla arguman  1, "cd: too many arguments"
**   HOME tanimsiz  1, "cd: HOME not set"
*/

#include "nax.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

/* Gidilecek hedefi argumandan ya da ortamdan secer; yoksa NULL doner. */
static const char	*pick_target(char **argv, int *print_it)
{
	const char	*target;

	*print_it = 0;
	if (argv[1] == NULL)
	{
		target = getenv("HOME");
		if (target == NULL || *target == '\0')
		{
			ex_warn_bi("cd", NULL, "HOME not set");
			return (NULL);
		}
		return (target);
	}
	if (strcmp(argv[1], "-") == 0)
	{
		target = getenv("OLDPWD");
		if (target == NULL || *target == '\0')
		{
			ex_warn_bi("cd", NULL, "OLDPWD not set");
			return (NULL);
		}
		*print_it = 1;
		return (target);
	}
	return (argv[1]);
}

/* Dizin degistikten sonra PWD ve OLDPWD degiskenlerini gunceller. */
static void	update_pwd(const char *old)
{
	char	buf[PATH_MAX];

	if (old != NULL)
		setenv("OLDPWD", old, 1);
	if (getcwd(buf, sizeof(buf)) != NULL)
		setenv("PWD", buf, 1);
}

/*
** Dizin degistirir.
**
** Hedef "-" ise yeni dizin basilir; bash da boyle davraniyor.
*/
int	bi_cd(t_shell *sh, char **argv)
{
	const char	*target;
	char		old[PATH_MAX];
	int			print_it;

	(void)sh;
	if (argv[1] != NULL && argv[2] != NULL)
	{
		ex_warn_bi("cd", NULL, "too many arguments");
		return (1);
	}
	target = pick_target(argv, &print_it);
	if (target == NULL)
		return (1);
	if (getcwd(old, sizeof(old)) == NULL)
		old[0] = '\0';
	if (chdir(target) != 0)
	{
		ex_warn_bi("cd", target, strerror(errno));
		return (1);
	}
	if (old[0] != '\0')
		update_pwd(old);
	else
		update_pwd(NULL);
	if (print_it)
		printf("%s\n", getenv("PWD"));
	return (0);
}
