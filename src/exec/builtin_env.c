/*
** builtin_env.c - ortam degiskeni yerlesikleri: export ve unset.
**
** KENDI DEPO YOK:
**   Bu kabuk ayri bir degisken deposu tutmuyor, dogrudan surec ortamini
**   kullaniyor (setenv/unsetenv). Bunun gorunur sonucu: disa
**   aktarilmamis kabuk degiskeni (X=1 gibi) desteklenmiyor ve
**   "export NAME" bicimi yapacak bir is bulamiyor. Ikisi de
**   docs/FINDINGS.md icinde kayitli.
**
** LISTELEME BICIMI:
**   Argumansiz "export" bash'te "declare -x NAME=..." basiyor, ama bu
**   kabukta declare diye bir sey yok. Bunun yerine geri yapistirilabilir
**   olan "export NAME=..." bicimi kullaniliyor; fark bilincli.
*/

#include "nax.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char	**environ;

/* Karakter degisken adinin ilk harfi olabilir mi. */
static int	is_name_start(char c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

/* Metnin basindan "=" ya da sonuna kadarki kismi gecerli bir ad mi. */
static int	is_valid_name(const char *s, size_t len)
{
	size_t	i;

	if (len == 0 || is_name_start(s[0]) == 0)
		return (0);
	i = 1;
	while (i < len)
	{
		if (is_name_start(s[i]) == 0 && (s[i] < '0' || s[i] > '9'))
			return (0);
		i++;
	}
	return (1);
}

/* Disa aktarilmis tum degiskenleri geri yapistirilabilir bicimde basar. */
static int	export_list(void)
{
	size_t	i;
	char	*eq;

	i = 0;
	while (environ[i] != NULL)
	{
		eq = strchr(environ[i], '=');
		if (eq != NULL)
			printf("export %.*s=\"%s\"\n", (int)(eq - environ[i]),
				environ[i], eq + 1);
		else
			printf("export %s\n", environ[i]);
		i++;
	}
	return (0);
}

/* Tek bir "NAME=deger" ya da "NAME" argumanini isler. */
static int	export_one(const char *arg)
{
	const char	*eq;
	char		name[256];
	size_t		len;

	eq = strchr(arg, '=');
	if (eq != NULL)
		len = (size_t)(eq - arg);
	else
		len = strlen(arg);
	if (len >= sizeof(name) || is_valid_name(arg, len) == 0)
	{
		ex_warn_bi("export", arg, "not a valid identifier");
		return (1);
	}
	if (eq == NULL)
		return (0);
	memcpy(name, arg, len);
	name[len] = '\0';
	if (setenv(name, eq + 1, 1) != 0)
	{
		ex_warn_bi("export", arg, "atanamadi");
		return (1);
	}
	return (0);
}

/* Argumansizken listeler, aksi halde her argumani ortama yazar. */
int	bi_export(t_shell *sh, char **argv)
{
	int	code;
	int	i;

	(void)sh;
	if (argv[1] == NULL)
		return (export_list());
	code = 0;
	i = 1;
	while (argv[i] != NULL)
	{
		if (export_one(argv[i]) != 0)
			code = 1;
		i++;
	}
	return (code);
}

/*
** Verilen adlari ortamdan siler.
**
** Gecersiz ad ve tanimsiz ad hata DEGIL: bash ikisinde de sessizce 0
** donuyor (olculdu).
*/
int	bi_unset(t_shell *sh, char **argv)
{
	int	i;

	(void)sh;
	i = 1;
	while (argv[i] != NULL)
	{
		if (is_valid_name(argv[i], strlen(argv[i])))
			unsetenv(argv[i]);
		i++;
	}
	return (0);
}
