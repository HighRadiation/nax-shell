/*
** line.c - satir okuma, prompt ve gecmis.
**
** NEDEN AYRI MODUL:
**   readline'in tum tuhafliklari (renk kaclarinin \001..\002 ile sarilmasi,
**   gecmis dosyasi, ileride oneriyi duzenleme tamponuna on-yukleme) tek
**   yerde kalsin. main.c "bana bir satir ver" demekten baska sey bilmez.
**
** RENK TUZAGI:
**   readline prompt genisligini sayarken basilmayan baytlari saymaz, ama
**   bunu bilmesi icin onlarin \001 ile \002 arasina alinmasi gerekir.
**   Sarilmazsa uzun satirlarda imlec kayar. CLR_* makrolari bu yuzden
**   kaclari zaten sarili tutar.
*/

#include "nax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>

/*
** Bir sonraki promptta tampona yazilacak metin.
**
** Dosya geneli tek bir isaretci, cunku readline'in baslangic kancasi
** parametre almiyor; hazirlanan metni kancaya baska yolla ulastirmak
** mumkun degil. Kanca metni yazdiktan sonra birakiyor ve NULL'a cekiyor,
** boylece oneri yalnizca BIR kez gelir.
*/
static char	*g_preload = NULL;

#define HIST_FILE ".nax_history"
#define CLR_NAME "\001\033[1;36m\002"
#define CLR_OFF "\001\033[0m\002"
#define PROMPT_EXTRA 64

/* HOME ile baslayan yolu ~ ile kisaltir; cagiran serbest birakir. */
static char	*shorten_path(const char *path)
{
	const char	*home;
	size_t		len;
	char		*out;

	home = getenv("HOME");
	if (home == NULL || *home == '\0')
		return (strdup(path));
	len = strlen(home);
	if (strncmp(path, home, len) != 0)
		return (strdup(path));
	if (path[len] != '\0' && path[len] != '/')
		return (strdup(path));
	out = malloc(strlen(path) - len + 2);
	if (out == NULL)
		return (NULL);
	out[0] = '~';
	memcpy(out + 1, path + len, strlen(path) - len + 1);
	return (out);
}

/* Calisma dizinini kisaltilmis haliyle dondurur; cagiran serbest birakir. */
static char	*current_dir(void)
{
	char	buf[PATH_MAX];

	if (getcwd(buf, sizeof(buf)) == NULL)
		return (strdup("?"));
	return (shorten_path(buf));
}

/* Gosterilecek prompt metnini uretir; cagiran serbest birakir. */
static char	*build_prompt(void)
{
	char	*dir;
	char	*prompt;
	size_t	size;

	dir = current_dir();
	if (dir == NULL)
		return (NULL);
	size = strlen(dir) + strlen(CLR_NAME) + strlen(CLR_OFF) + PROMPT_EXTRA;
	prompt = malloc(size);
	if (prompt == NULL)
	{
		free(dir);
		return (NULL);
	}
	snprintf(prompt, size, "%s%s%s %s $ ", CLR_NAME, NAX_NAME, CLR_OFF, dir);
	free(dir);
	return (prompt);
}

/* readline'i bu kabuk adina tanitir; ~/.inputrc kurallari buna gore isler. */
void	ln_setup(void)
{
	rl_readline_name = NAX_NAME;
}

/* Gecmis dosyasinin tam yolunu uretir; HOME yoksa NULL doner. */
char	*ln_hist_path(void)
{
	const char	*home;
	char		*path;
	size_t		size;

	home = getenv("HOME");
	if (home == NULL || *home == '\0')
		return (NULL);
	size = strlen(home) + strlen(HIST_FILE) + 2;
	path = malloc(size);
	if (path == NULL)
		return (NULL);
	snprintf(path, size, "%s/%s", home, HIST_FILE);
	return (path);
}

/* Varsa gecmis dosyasini readline'a yukler. */
void	ln_hist_load(const char *path)
{
	if (path == NULL)
		return ;
	read_history(path);
}

/* Gecmisi diske yazar; yol uretilememisse hicbir sey yapmaz. */
void	ln_hist_save(const char *path)
{
	if (path == NULL)
		return ;
	write_history(path);
}

/*
** Etkilesimsiz girdide bir satir okur; sondaki yeni satiri atar.
**
** NEDEN readline DEGIL:
**   readline stdin bir terminal olmadiginda okudugu satiri stdout'a
**   yankilar. Bu, betik ve test ciktisini ikiye katlar. Duzenleme
**   yetenegi de zaten yalnizca terminalde anlamli oldugu icin
**   etkilesimsiz yol dogrudan getline kullanir.
*/
static char	*read_plain(void)
{
	char	*line;
	size_t	cap;
	ssize_t	len;

	line = NULL;
	cap = 0;
	len = getline(&line, &cap, stdin);
	if (len < 0)
	{
		free(line);
		return (NULL);
	}
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';
	return (line);
}

/*
** Bir gecmis satirinin ilk sozcugu verilen ad mi.
**
** Tam satiri ayirmak gerekmiyor: yalniz basi karsilastirmak yeterli ve
** gecmis her aday icin taraniyor, o yuzden ucuz olmasi onemli.
*/
static int	head_matches(const char *line, const char *name, size_t len)
{
	while (*line == ' ' || *line == '\t')
		line++;
	if (strncmp(line, name, len) != 0)
		return (0);
	return (line[len] == '\0' || line[len] == ' ' || line[len] == '\t');
}

/*
** Gecmiste bu adin kac kez bas olarak kullanildigini verir.
**
** NEDEN BURADA: gecmis readline'in elinde. Yazim duzeltmesi bu sayiyi
** esit uzaklikta hangi adayin kazanacagina karar vermek icin kullaniyor
** ama readline'i tanimasi gerekmiyor; bu islev araya giriyor.
**
** Etkilesimli olmayan kosumda gecmis bos olur ve her aday icin 0 doner;
** o zaman siralamayi tarama sirasi belirler. Testlerin belirleyici
** olmasinin sebebi de bu.
*/
int	ln_head_uses(const char *name)
{
	HIST_ENTRY	**list;
	size_t		len;
	int			count;
	int			i;

	list = history_list();
	if (list == NULL || name == NULL || *name == '\0')
		return (0);
	len = strlen(name);
	count = 0;
	i = 0;
	while (list[i] != NULL)
	{
		if (list[i]->line != NULL && head_matches(list[i]->line, name, len))
			count++;
		i++;
	}
	return (count);
}

/*
** Bir sonraki promptta duzenleme tamponuna hazir gelecek metni saklar.
**
** NEDEN ISARETCI DEGIL KOPYA: cagiran metni kendi yigitinda tutuyor,
** prompt ise bir sonraki dongude basiliyor. Isaretciyi saklamak askida
** kalan bellege bakmak olurdu.
*/
void	ln_preload(const char *text)
{
	free(g_preload);
	g_preload = NULL;
	if (text != NULL && *text != '\0')
		g_preload = strdup(text);
}

/*
** readline tamponu hazirlanirken bekleyen metni icine yazar.
**
** rl_startup_hook, tampon olusturulduktan ama kullanici tusa basmadan
** once cagriliyor; metni buraya yazmak kullanicinin kendisi yazmis
** olmasiyla ayni sonucu veriyor, yani Enter yeterli oluyor.
*/
static int	insert_preload(void)
{
	if (g_preload == NULL)
		return (0);
	rl_insert_text(g_preload);
	free(g_preload);
	g_preload = NULL;
	return (0);
}

/* Terminalde prompt basip bir satir okur ve bos degilse gecmise ekler. */
static char	*read_interactive(void)
{
	char	*prompt;
	char	*line;

	prompt = build_prompt();
	rl_startup_hook = insert_preload;
	if (prompt != NULL)
		line = readline(prompt);
	else
		line = readline("");
	free(prompt);
	if (line != NULL && *line != '\0')
		add_history(line);
	return (line);
}

/* Bir satir okur; cagiran serbest birakir. EOF'ta NULL doner. */
char	*ln_read(const t_shell *sh)
{
	if (sh->interactive == 0)
		return (read_plain());
	return (read_interactive());
}
