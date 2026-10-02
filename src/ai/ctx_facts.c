/*
** ctx_facts.c - oturumun degismeyen ve yavas degisen olgulari.
**
** NE GONDERILIR, NE GONDERILMEZ:
**   Liste docs/PRIVACY.md'de yazili ve bu dosya o listeyi uyguluyor.
**   Gonderilen: calisma dizini (ev dizini kisaltilmis), git dali,
**   terminal genisligi, isletim sistemi, dil ayari, en sik kullanilan
**   komut baslari ve ortam degiskenlerinin ADLARI.
**
**   Gonderilmeyen: dizin listesi, dosya icerigi, program hata ciktisi ve
**   ortam degiskenlerinin DEGERLERI.
**
** ORTAM DEGISKENLERI: YALNIZCA ADLAR
**   Adlar modelin ortami anlamasina yetiyor ("bu makinede sanal ortam
**   var") ama degerler sir tasiyabiliyor. Ad listesi yalnizca HELLO ile
**   bir kez gidiyor: her istekte yuz isim gondermek jetonu bosa
**   harcamak olurdu.
**
** GIT DALI DOSYADAN OKUNUYOR, KOMUTLA DEGIL:
**   ".git/HEAD" okumak bir alt surec catallamaktan cok daha hizli ve her
**   istekte yapiliyor. Dizinin temiz olup olmadigi ise alt surec
**   gerektiriyor; o yuzden su an gonderilmiyor ve FINDINGS'te kayitli.
*/

#include "nax.h"
#include "ai.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>

extern char	**environ;

#define CTX_TOP_HEADS 6
#define CTX_MAX_ENV 400

/* Anahtar ve degeri "anahtar: deger" bicimiyle yazar. */
static int	push_fact(t_buf *buf, const char *key, const char *value)
{
	if (value == NULL || *value == '\0')
		return (1);
	if (buf_push_str(buf, key) == 0 || buf_push_str(buf, ": ") == 0)
		return (0);
	if (buf_push_str(buf, value) == 0)
		return (0);
	return (buf_push(buf, '\n'));
}

/*
** Calisma dizininden yukari dogru ".git/HEAD" arar ve dal adini verir.
**
** Ayrik HEAD halinde dosyada dal adi yerine bir ozet duruyor; o durumda
** ilk yedi karakter veriliyor, cunku "hangi dalda" sorusunun cevabi
** "hicbirinde" ise bunu gormek de bilgi.
*/
static char	*git_branch(void)
{
	char	path[1024];
	char	line[256];
	FILE	*fp;
	size_t	len;
	char	*cut;

	if (getcwd(path, sizeof(path) - 32) == NULL)
		return (NULL);
	while (1)
	{
		len = strlen(path);
		snprintf(path + len, sizeof(path) - len, "/.git/HEAD");
		fp = fopen(path, "r");
		path[len] = '\0';
		if (fp != NULL)
			break ;
		cut = strrchr(path, '/');
		if (cut == NULL || cut == path)
			return (NULL);
		*cut = '\0';
	}
	cut = fgets(line, sizeof(line), fp);
	fclose(fp);
	if (cut == NULL)
		return (NULL);
	len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
		line[--len] = '\0';
	if (strncmp(line, "ref: refs/heads/", 16) == 0)
		return (strdup(line + 16));
	line[7] = '\0';
	return (strdup(line));
}

/* Terminal genisligini verir; olculemezse 0. */
static int	term_width(void)
{
	struct winsize	ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0)
		return (0);
	return (ws.ws_col);
}

/* Isletim sistemi adini verir; cagiran birakir. */
static char	*os_name(void)
{
	struct utsname	info;

	if (uname(&info) != 0)
		return (NULL);
	return (strdup(info.sysname));
}

/*
** Ortam degiskenlerinin ADLARINI tampona yazar.
**
** Deger hicbir kosulda yazilmiyor: esittir isaretinden sonrasi
** atlaniyor. Sayi sinirli, cunku cok sayida degiskeni olan bir makinede
** liste baglamin tamamini bogabilir.
*/
static int	push_env_names(t_buf *buf)
{
	size_t	i;
	size_t	len;
	char	name[128];

	if (environ == NULL || environ[0] == NULL)
		return (1);
	if (buf_push_str(buf, "ortam degiskeni adlari: ") == 0)
		return (0);
	i = 0;
	while (environ[i] != NULL && i < CTX_MAX_ENV)
	{
		len = strcspn(environ[i], "=");
		if (len > 0 && len + 1 < sizeof(name))
		{
			memcpy(name, environ[i], len);
			name[len] = '\0';
			if (i > 0 && buf_push_str(buf, ", ") == 0)
				return (0);
			if (buf_push_str(buf, name) == 0)
				return (0);
		}
		i++;
	}
	return (buf_push(buf, '\n'));
}

/* Sayiyi metne cevirip olgu olarak yazar. */
static int	push_number(t_buf *buf, const char *key, int value)
{
	char	text[32];

	if (value <= 0)
		return (1);
	snprintf(text, sizeof(text), "%d", value);
	return (push_fact(buf, key, text));
}

/* Olgulari yazar ve ayrilan metinleri birakir. */
static int	push_all(t_buf *buf, char *cwd, char *branch, char *os,
		char *heads)
{
	int	ok;

	ok = push_fact(buf, "dizin", cwd);
	ok = ok && push_fact(buf, "git dali", branch);
	ok = ok && push_fact(buf, "isletim sistemi", os);
	ok = ok && push_fact(buf, "dil", getenv("LANG"));
	ok = ok && push_number(buf, "terminal genisligi", term_width());
	ok = ok && push_fact(buf, "en sik kullanilan", heads);
	free(cwd);
	free(branch);
	free(os);
	free(heads);
	return (ok);
}

/*
** Oturum olgularini tampona yazar; basarida 1.
**
** Ortam degiskeni adlari BURADA DEGIL: liste uzun ve yalnizca oturum
** acilisinda bir kez gonderiliyor. Her istekte tasimak jetonu bosa
** harcamak olurdu.
*/
int	ctx_push_facts(t_buf *buf)
{
	char	path[1024];
	char	*cwd;
	char	*branch;
	char	*os;
	char	*heads;

	cwd = NULL;
	if (getcwd(path, sizeof(path)) != NULL)
		cwd = ln_short_path(path);
	branch = git_branch();
	os = os_name();
	heads = ln_top_heads(CTX_TOP_HEADS);
	return (push_all(buf, cwd, branch, os, heads));
}

/* Oturum acilisinda bir kez gonderilen genis anlik goruntuyu kurar. */
char	*ctx_hello(void)
{
	t_buf	buf;

	buf_init(&buf);
	if (ctx_push_facts(&buf) == 0 || push_env_names(&buf) == 0)
	{
		buf_free(&buf);
		return (NULL);
	}
	return (buf_take(&buf));
}

/*
** Ortam degiskeni adlarini tek metin olarak verir; cagiran birakir.
**
** NEDEN AYRICA DISA ACILIYOR: "nax ctx" komutu gidecek her seyi
** gostermek zorunda. Bu liste yalnizca oturum acilisinda gidiyor ama
** gitmiyor demek degil - gosterilmezse gonderilenin en buyuk parcasi
** denetlenemez kalirdi ve gizlilik sozu bozulurdu.
*/
char	*ctx_env_names(void)
{
	t_buf	buf;

	buf_init(&buf);
	if (push_env_names(&buf) == 0)
	{
		buf_free(&buf);
		return (NULL);
	}
	if (buf.data == NULL)
		return (strdup(""));
	return (buf_take(&buf));
}
