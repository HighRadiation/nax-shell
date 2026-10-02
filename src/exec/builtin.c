/*
** builtin.c - yerlesik komut tablosu ve kabuk duzeyi yerlesikler.
**
** YERLESIK NEREDE KOSAR:
**   Tek basina bir yerlesik ANA surecte kosar, cunku cd, export, unset ve
**   exit kabugun durumunu degistirir; cocukta kosarlarsa degisiklik
**   cocukla birlikte yok olur ve "cd /tmp" hicbir ise yaramaz.
**
**   Boru hattinin ICINDEKI yerlesik ise cocukta kosar ve bu da dogru:
**   bash'te de "echo x | cd /tmp" kabugun dizinini degistirmiyor
**   (olculdu). Yani kural bir eksiklik degil, standart davranis.
**
** COZUMLEME GENISLETMEDEN SONRA:
**   Yerlesik olup olmadigina argv[0] GENISLETILDIKTEN sonra bakilir.
**   Boylece CMD=cd iken "$CMD /tmp" de calisir; bash da boyle davraniyor.
**
** "env" NEDEN YOK:
**   Plan listesinde vardi ama yerlesik olarak yazmak bizi bash'ten
**   UZAKLASTIRIRDI: /usr/bin/env zaten PATH'te ve ortami birebir ayni
**   basiyor, cunku bu kabuk kendi degisken deposunu tutmuyor, dogrudan
**   surec ortamini kullaniyor. Yerlesik yapmanin tek sonucu "env a b"
**   gibi kullanimlarda bash'ten farkli davranmak olurdu.
*/

#include "nax.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define EXIT_STATUS_MAX 256

/* Yerlesik tablosunu dondurur; iki okuyucu ayni listeyi gorsun diye ayri. */
static const t_builtin	*bi_table(void)
{
	static const t_builtin	table[] = {
		{"cd", bi_cd},
		{"echo", bi_echo},
		{"pwd", bi_pwd},
		{"export", bi_export},
		{"unset", bi_unset},
		{"exit", bi_exit},
		{"ctx", bi_ctx},
		{NULL, NULL}
	};

	return (table);
}

/*
** Siradaki yerlesigin adini dondurur; liste bittiginde NULL.
**
** Yazim duzeltmesi aday olarak yerlesikleri de taramak zorunda, yoksa
** "ehco" icin PATH'te karsilik bulunur ama "exprot" icin bulunmaz.
*/
const char	*bi_name_at(size_t i)
{
	const t_builtin	*table;
	size_t			n;

	table = bi_table();
	n = 0;
	while (table[n].name != NULL)
		n++;
	if (i >= n)
		return (NULL);
	return (table[i].name);
}

/* Ada karsilik gelen yerlesigi dondurur; yoksa NULL. */
t_builtin_fn	bi_lookup(const char *name)
{
	const t_builtin	*table;
	size_t			i;

	table = bi_table();
	i = 0;
	while (table[i].name != NULL)
	{
		if (strcmp(table[i].name, name) == 0)
			return (table[i].fn);
		i++;
	}
	return (NULL);
}

/*
** Arguman "-n" bayragi mi.
**
** Bash tire ve ARDINDAN en az bir n disinda hicbir sey gelmeyen argumani
** bayrak sayiyor: "-nn" bayrak, "-n-n" ve "--n" degil (olculdu).
*/
static int	is_no_newline_flag(const char *arg)
{
	size_t	i;

	if (arg[0] != '-' || arg[1] != 'n')
		return (0);
	i = 1;
	while (arg[i] == 'n')
		i++;
	return (arg[i] == '\0');
}

/* Argumanlari bosluklarla ayirip basar; -n verilmisse yeni satir yazmaz. */
int	bi_echo(t_shell *sh, char **argv)
{
	int	i;
	int	newline;

	(void)sh;
	newline = 1;
	i = 1;
	while (argv[i] != NULL && is_no_newline_flag(argv[i]))
	{
		newline = 0;
		i++;
	}
	while (argv[i] != NULL)
	{
		printf("%s", argv[i]);
		i++;
		if (argv[i] != NULL)
			printf(" ");
	}
	if (newline)
		printf("\n");
	return (0);
}

/* Calisma dizinini basar; fazla arguman yok sayilir (bash da oyle). */
int	bi_pwd(t_shell *sh, char **argv)
{
	char	buf[PATH_MAX];

	(void)sh;
	(void)argv;
	if (getcwd(buf, sizeof(buf)) == NULL)
	{
		ex_warn_bi("pwd", NULL, strerror(errno));
		return (1);
	}
	printf("%s\n", buf);
	return (0);
}

/*
** Metni cikis koduna cevirir; sayi degilse 0 doner.
**
** Isaret kabul edilir ve deger 256'ya gore indirgenir: bash "exit 300"
** icin 44, "exit -1" icin 255, "exit -44" icin 212 veriyor (sekiz deger
** olculup dogrulandi).
**
** Dongu ICINDE de indirgeme var; bu tasma korumasi, cok uzun sayi
** dizileri long'u asmasin diye. Sondaki indirgeme yalnizca isaret icin
** gerekli.
**
** DURUST NOT: isletim sistemi cikis kodunu zaten kirpiyor, yani bu
** indirgeme kaldirilsa gozlenebilir davranis DEGISMEZ - mutasyon
** denemesi bunu gosterdi, hicbir vaka patlamadi. Yine de duruyor, cunku
** niyeti kodda gormek "-44 dondur, isletim sistemi halleder"den iyi.
*/
static int	parse_status(const char *s, int *out)
{
	long	value;
	int		sign;
	size_t	i;

	sign = 1;
	i = 0;
	if (s[0] == '-' || s[0] == '+')
	{
		if (s[0] == '-')
			sign = -1;
		i = 1;
	}
	if (s[i] == '\0')
		return (0);
	value = 0;
	while (s[i] != '\0')
	{
		if (s[i] < '0' || s[i] > '9')
			return (0);
		value = (value * 10 + (s[i] - '0')) % EXIT_STATUS_MAX;
		i++;
	}
	value *= sign;
	*out = (int)((value % EXIT_STATUS_MAX + EXIT_STATUS_MAX)
			% EXIT_STATUS_MAX);
	return (1);
}

/*
** Kabuktan cikar.
**
** Argumansizken son durum kullanilir. Sayisal olmayan arguman hatadir ve
** kabuk 2 ile CIKAR; fazla arguman ise hatadir ama kabuk CIKMAZ ve 1
** doner. Iki davranisin farki bash olculerek dogrulandi.
*/
int	bi_exit(t_shell *sh, char **argv)
{
	int	code;

	if (argv[1] == NULL)
	{
		sh->exiting = 1;
		return (sh->last_status);
	}
	if (parse_status(argv[1], &code) == 0)
	{
		ex_warn_bi("exit", argv[1], "numeric argument required");
		sh->exiting = 1;
		return (2);
	}
	if (argv[2] != NULL)
	{
		ex_warn_bi("exit", NULL, "too many arguments");
		return (1);
	}
	sh->exiting = 1;
	return (code);
}
