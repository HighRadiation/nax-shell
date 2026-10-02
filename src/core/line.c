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

/*
** Yolu ev dizini "~" ile kisaltir; cagiran serbest birakir.
**
** IKI KULLANICISI VAR: prompt ve baglam anlik goruntusu. Gizlilik
** sozlesmesi calisma dizininin "~ ile kisaltilmis" halde gonderilmesini
** soyluyor, yani ayni kisaltma iki yerde gerekiyor. Burada durmasinin
** sebebi prompt'un ilk kullanici olmasi.
*/
char	*ln_short_path(const char *path)
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
	return (ln_short_path(buf));
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

/* Satirin ilk sozcugunu tampona yazar; sozcuk yoksa 0 doner. */
static int	head_of(const char *line, char *out, size_t cap)
{
	size_t	n;

	if (line == NULL)
		return (0);
	while (*line == ' ' || *line == '\t')
		line++;
	n = 0;
	while (line[n] != '\0' && line[n] != ' ' && line[n] != '\t'
		&& n + 1 < cap)
		n++;
	if (n == 0)
		return (0);
	memcpy(out, line, n);
	out[n] = '\0';
	return (1);
}

/* Adi tabloda arar; yoksa -1 doner. */
static int	find_head(char *names[], size_t used, const char *head)
{
	size_t	i;

	i = 0;
	while (i < used)
	{
		if (strcmp(names[i], head) == 0)
			return ((int)i);
		i++;
	}
	return (-1);
}

/*
** Gecmisteki komut baslarini sayar; tabloya kac ayri ad girdigini verir.
**
** NEDEN BURADA: gecmis readline'in elinde. Gizlilik sozlesmesi bu listeyi
** gonderilenler arasinda sayiyor, cunku hangi araclari kullandigini bilen
** bir model daha isabetli komut uretir.
**
** TABLO DOLARSA YENI ADLAR YOK SAYILIR: gecmisi olan bir kullanicida ilk
** altmis dort farkli komut zaten en siklarini iceriyor ve siniri
** buyutmenin karsiligi yok.
*/
static size_t	count_heads(char *names[], int counts[], size_t cap)
{
	HIST_ENTRY	**list;
	char		head[64];
	size_t		used;
	size_t		i;
	int			at;

	list = history_list();
	used = 0;
	if (list == NULL)
		return (0);
	i = 0;
	while (list[i] != NULL)
	{
		if (head_of(list[i]->line, head, sizeof(head)))
		{
			at = find_head(names, used, head);
			if (at >= 0)
				counts[at]++;
			else if (used < cap && (names[used] = strdup(head)) != NULL)
				counts[used++] = 1;
		}
		i++;
	}
	return (used);
}

/* Tabloda en yuksek sayiyi tasiyan kaydi verir; kalmamissa -1. */
static int	best_unused(int counts[], size_t used)
{
	size_t	i;
	int		best;

	best = -1;
	i = 0;
	while (i < used)
	{
		if (counts[i] > 0 && (best < 0 || counts[i] > counts[best]))
			best = (int)i;
		i++;
	}
	return (best);
}

/* En sik kullanilan ilk "top" basi tampona yazar. */
static void	pick_top(t_buf *buf, char *names[], int counts[], size_t used,
		size_t top)
{
	char	piece[96];
	size_t	taken;
	int		at;

	taken = 0;
	while (taken < top)
	{
		at = best_unused(counts, used);
		if (at < 0)
			return ;
		snprintf(piece, sizeof(piece), "%s%s %d", taken ? ", " : "",
			names[at], counts[at]);
		if (buf_push_str(buf, piece) == 0)
			return ;
		counts[at] = 0;
		taken++;
	}
}

/* Tabloda ayrilan adlari birakir. */
static void	free_names(char *names[], size_t used)
{
	size_t	i;

	i = 0;
	while (i < used)
	{
		free(names[i]);
		i++;
	}
}

/*
** En sik kullanilan baslari "ad sayi, ad sayi" bicimiyle verir.
**
** Cagiran serbest birakir. Gecmis bossa bos dize doner.
*/
char	*ln_top_heads(size_t top)
{
	char	*names[64];
	int		counts[64];
	t_buf	buf;
	size_t	used;

	used = count_heads(names, counts, 64);
	buf_init(&buf);
	pick_top(&buf, names, counts, used, top);
	free_names(names, used);
	if (buf.data == NULL)
		return (strdup(""));
	return (buf_take(&buf));
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
