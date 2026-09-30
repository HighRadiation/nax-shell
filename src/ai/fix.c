/*
** fix.c - yerel yazim duzeltmesi: "celar" icin AI'a hic gitmeden "clear".
**
** NEDEN AI'DAN ONCE:
**   Cozulmeyen basin en sik sebebi yazim hatasi. Bunu bir ag turuna
**   baglamak hem gecikme hem ucret demek, oysa cevap tamamen yerel.
**
** UZAKLIK OLCUSU - NEDEN YER DEGISTIRME DE SAYILIYOR:
**   Duz Levenshtein'de "celar" ile "clear" arasi 2 adim. En sik daktilo
**   hatasi ise tam olarak komsu iki harfin yer degistirmesi. Yer
**   degistirmeyi tek adim sayan olcu (Damerau-Levenshtein) bu hatalari
**   esigin altinda tutuyor, esigi buyutmek zorunda kalmadan.
**
** UC SINIR VE HEPSININ SEBEBI:
**   FIX_MIN_LEN  Kisa bas icin duzeltme DENENMEZ. "a" sozcugunun PATH'te
**                uzakligi 1 olan onlarca karsiligi var; oneri gurultuye
**                donusur ve "a && b" gibi satirlar bozulur.
**   FIX_MAX_LEN  Cok uzun sozcuk denenmez; DP tablosu sabit boyutlu,
**                cunku degisken uzunluklu dizi yasak.
**   esik         Uzunluga gore 1 ya da 2; kisa sozcukte 2 adim cok
**                genis, tek harfi ortak olan her seyi aday yapiyor.
**
** CAGIRMA KOSULU:
**   Bu islev YALNIZCA satir kabuk seklindeyken cagrilir, yani hicbir
**   sekil vetosu tetiklenmemisken. Sebebi olculdu: "dun degisen
**   dosyalari goster" satirinda "dun" sozcugunun "du" komutuna uzakligi
**   1. Kosul olmasa Turkce istekler disk kullanimi komutu sanilirdi.
*/

#include "nax.h"
#include "ai.h"
#include "exec.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#define FIX_MIN_LEN 3

/* Uc sayidan en kucugunu dondurur. */
static int	min3(int a, int b, int c)
{
	if (b < a)
		a = b;
	if (c < a)
		a = c;
	return (a);
}

/* DP tablosunun ilk satir ve ilk kolonunu doldurur. */
static void	dl_init(int d[][FIX_MAX_LEN + 1], size_t la, size_t lb)
{
	size_t	i;

	i = 0;
	while (i <= la)
	{
		d[i][0] = (int)i;
		i++;
	}
	i = 0;
	while (i <= lb)
	{
		d[0][i] = (int)i;
		i++;
	}
}

/* Bir hucreyi doldurur; komsu iki harf yer degistirmisse tek adim sayar. */
static void	dl_cell(int d[][FIX_MAX_LEN + 1], const char *a, const char *b,
		size_t i)
{
	size_t	j;
	int		cost;

	j = 1;
	while (b[j - 1] != '\0')
	{
		cost = (a[i - 1] != b[j - 1]);
		d[i][j] = min3(d[i - 1][j] + 1, d[i][j - 1] + 1,
				d[i - 1][j - 1] + cost);
		if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]
			&& d[i - 2][j - 2] + 1 < d[i][j])
			d[i][j] = d[i - 2][j - 2] + 1;
		j++;
	}
}

/* Iki sozcuk arasindaki uzakligi verir; siniri asan sozcukte -1 doner. */
int	fix_distance(const char *a, const char *b)
{
	int		d[FIX_MAX_LEN + 1][FIX_MAX_LEN + 1];
	size_t	la;
	size_t	lb;
	size_t	i;

	la = strlen(a);
	lb = strlen(b);
	if (la > FIX_MAX_LEN || lb > FIX_MAX_LEN)
		return (-1);
	dl_init(d, la, lb);
	i = 1;
	while (i <= la)
	{
		dl_cell(d, a, b, i);
		i++;
	}
	return (d[la][lb]);
}

/* Sozcuk uzunluguna gore kabul edilen en buyuk uzaklik. */
static int	fix_limit(const char *word)
{
	if (strlen(word) <= 4)
		return (1);
	return (2);
}

/*
** Aday daha iyiyse kaydi gunceller; esitlikte gecmis sikligi belirler.
**
** Gecmis sayimi esige GIREN adaylar icin yapiliyor. Her dizin girdisi icin
** yapilsa tarama, girdi sayisi carpi gecmis uzunlugu kadar is olurdu;
** oysa esige giren aday sayisi tek haneli.
**
** UZUNLUK KONTROLU BILINCLI OLARAK GEREKSIZ: fix_distance zaten esikten
** uzun adlara -1 donuyor ve o da yukaridaki satirda eleniyor. Yine
** duruyor, cunku hemen altindaki memcpy'nin on kosulu bu ve bir bellek
** yazmasinin guvenligini uzaktaki bir islevin sozlesmesine BAGLAMAK
** istemiyorum. Testle gozlemlenemez; FINDINGS.md icinde kayitli.
*/
static void	fix_offer(t_fix *best, const char *name, int distance)
{
	size_t	len;
	int		uses;

	if (distance < 0 || distance > best->limit)
		return ;
	if (best->found && distance > best->distance)
		return ;
	len = strlen(name);
	if (len > FIX_MAX_LEN)
		return ;
	uses = ln_head_uses(name);
	if (best->found && distance == best->distance && uses <= best->uses)
		return ;
	memcpy(best->name, name, len);
	best->name[len] = '\0';
	best->distance = distance;
	best->uses = uses;
	best->found = 1;
}

/* Yerlesikleri aday olarak tarar. */
static void	scan_builtins(t_fix *best, const char *word)
{
	const char	*name;
	size_t		i;

	i = 0;
	name = bi_name_at(i);
	while (name != NULL)
	{
		fix_offer(best, name, fix_distance(word, name));
		i++;
		name = bi_name_at(i);
	}
}

/* Tek bir PATH dizinini tarar. */
static void	scan_dir(t_fix *best, const char *word, const char *dir)
{
	DIR				*dp;
	struct dirent	*entry;

	dp = opendir(dir);
	if (dp == NULL)
		return ;
	entry = readdir(dp);
	while (entry != NULL)
	{
		if (entry->d_name[0] != '.')
			fix_offer(best, entry->d_name, fix_distance(word, entry->d_name));
		entry = readdir(dp);
	}
	closedir(dp);
}

/* PATH'i iki nokta ile ayirip her dizini tarar. */
static void	scan_path(t_fix *best, const char *word)
{
	char	*path;
	char	*copy;
	char	*dir;

	path = getenv("PATH");
	if (path == NULL || *path == '\0')
		return ;
	copy = strdup(path);
	if (copy == NULL)
		return ;
	dir = strtok(copy, ":");
	while (dir != NULL)
	{
		scan_dir(best, word, dir);
		dir = strtok(NULL, ":");
	}
	free(copy);
}

/*
** Cozulmeyen bas icin yerel oneri arar.
**
** Bulursa 1 doner ve oneriyi best->name icine yazar. Bulunan ad her zaman
** yeniden dogrulanir: PATH dizininde okunabilen ama calistirilamayan
** dosyalar var ve bunlari komut olarak onermek yanlis olur.
**
** BURADA "uzaklik sifirsa vazgec" KONTROLU YOK. Ilk yazimda vardi;
** mutasyon testi silmenin hicbir testi bozmadigini gosterdi ve sebebi
** olculdu: uzakligi sifir olan aday, adi aynen tasiyan ama
** calistirilamayan bir PATH dosyasi olabilir ANCAK, cunku yerlesikler ve
** calistirilabilir dosyalar basi zaten cozerdi. O hali de asagidaki
** dogrulama zaten eliyor. Iki korumadan biri gereksizdi.
*/
int	fix_suggest(const char *word, t_fix *best)
{
	best->from[0] = '\0';
	best->name[0] = '\0';
	best->distance = 0;
	best->uses = 0;
	best->found = 0;
	best->limit = fix_limit(word);
	if (strlen(word) < FIX_MIN_LEN || strlen(word) > FIX_MAX_LEN)
		return (0);
	memcpy(best->from, word, strlen(word) + 1);
	scan_builtins(best, word);
	scan_path(best, word);
	if (best->found && bi_lookup(best->name) == NULL
		&& path_is_command(best->name) == 0)
		best->found = 0;
	return (best->found);
}

/*
** Bu ad geri donusu olmayan bir komut mu.
**
** NEDEN VAR: oneri kabul edildiginde bir sonraki prompta HAZIR gelir,
** yani kullanici yalniz Enter'a basar. "rn -rf ." satiri icin "rm -rf ."
** onerip duzenleme tamponuna yazmak, tek refleks tusla geri donusu
** olmayan bir silme demek. Bu adlar icin oneri YAZILIR ama tampona
** KONULMAZ; kullanici kendisi yazar.
**
** Liste kasten kisa ve yalniz geri alinamayan islere bakiyor: dosya
** silen, uzerine yazan, izin ve sahiplik degistiren, surec olduren.
*/
int	fix_is_dangerous(const char *name)
{
	static const char	*const risky[] = {
		"rm", "rmdir", "dd", "shred", "mkfs", "mkfs.ext4", "mkfs.xfs",
		"mkswap", "fdisk", "parted", "chown", "chgrp", "chmod", "kill",
		"killall", "pkill", "mv", "truncate", "reboot", "shutdown",
		"halt", "poweroff", "userdel", "groupdel", NULL
	};
	size_t				i;

	i = 0;
	while (risky[i] != NULL)
	{
		if (strcmp(risky[i], name) == 0)
			return (1);
		i++;
	}
	return (0);
}

/*
** Satirin basini oneriyle degistirip yeni satiri dondurur; cagiran birakir.
**
** Eslesmezse NULL doner ve cagiran oneriyi tampona koymaz. NEDEN BOYLE:
** token listesi satirdaki konumu tasimiyor, o yuzden bas ancak satirin
** basinda AYNEN geciyorsa guvenle degistirilebilir. Tirnakli bir bas
** ("celar" gibi) bu kosulu bozar; o halde oneri yalnizca yazilir.
*/
char	*fix_rewrite(const char *line, const char *head, const char *name)
{
	t_buf	buf;
	size_t	len;

	while (*line == ' ' || *line == '\t')
		line++;
	len = strlen(head);
	if (len == 0 || strncmp(line, head, len) != 0)
		return (NULL);
	buf_init(&buf);
	if (buf_push_str(&buf, name) == 0 || buf_push_str(&buf, line + len) == 0)
	{
		buf_free(&buf);
		return (NULL);
	}
	return (buf.data);
}
