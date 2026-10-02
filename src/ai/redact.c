/*
** redact.c - gonderilecek metinden sirlari temizler.
**
** NEDEN KABUK TARAFINDA:
**   Temizleme veri yardimci surece VERILMEDEN once yapiliyor. Gerekcesi
**   basit: hatali ya da ele gecirilmis bir yardimci surec, hic almadigi
**   veriyi sizdiramaz. Temizlemeyi gonderen tarafa koymak, guvenmek
**   zorunda oldugun kod miktarini kucultuyor.
**
** NEDEN YER TUTUCU BIRAKILIYOR:
**   Eslesen kisim silinmiyor, "[GIZLI:tur]" ile degistiriliyor. Model
**   orada bir sir oldugunu bilir, ne oldugunu bilmez. Boylece "bu komut
**   neden basarisiz oldu" sorusunu yanitlarken eksik bilgiyle uydurmak
**   zorunda kalmiyor - silinmis bir anahtarin yerinde hicbir sey
**   olmasaydi komut anlamsiz gorunurdu.
**
** TARAMA TEK GECISTE:
**   Her konumda kurallar siraya gore denenir; biri eslesirse yer tutucu
**   yazilir ve eslesen bolum atlanir. Duzenli ifade kutuphanesi yok,
**   cunku her kural birkac satirlik elle yazilmis bir esleyici ve bir
**   kutuphane vendor etmek bu kazanca degmez.
**
** KISA SECENEKLER BILINCLI OLARAK KAPSAM DISI:
**   "-p DEGER" bicimi taranmiyor. Sebebi olculdu: "mkdir -p dizin"
**   satirinda "dizin" parola sanilirdi ve baglam bozulurdu. Yanlis
**   maskeleme sessizce yanlis cevap uretir; eksik maskeleme ise
**   FINDINGS'te kayitli bilinen bir sinirdir.
*/

#include "nax.h"
#include "ai.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define REDACT_LONG_RUN 32

/* Karakter onaltilik basamak mi. */
static int	is_hex(char c)
{
	return (isdigit((unsigned char)c) || (c >= 'a' && c <= 'f')
		|| (c >= 'A' && c <= 'F'));
}

/* Karakter base64 alfabesinde mi. */
static int	is_b64ish(char c)
{
	return (isalnum((unsigned char)c) || c == '+' || c == '/' || c == '='
		|| c == '-' || c == '_');
}

/* Sozcuk basi mi; onceki karakter ad karakteri olmamali. */
static int	at_word_start(const char *text, const char *at)
{
	if (at == text)
		return (1);
	return (!isalnum((unsigned char)at[-1]) && at[-1] != '_');
}

/* Verilen konumdan baslayan ad karakteri olmayan ilk yere kadarki uzunluk. */
static size_t	token_len(const char *at)
{
	size_t	n;

	n = 0;
	while (at[n] != '\0' && is_b64ish(at[n]) && at[n] != '=')
		n++;
	return (n);
}

/*
** Bilinen anahtar onekleri.
**
** Liste kisa ve kasten yaygin saglayicilarla sinirli. Her onek kendi
** sahibinin belgesinden alindi; uydurma onek eklemek yanlis maskeleme
** riski demek.
*/
static size_t	match_prefix(const char *at)
{
	static const char	*const	heads[] = {
		"sk-ant-", "sk-", "gsk_", "ghp_", "gho_", "ghs_", "glpat-",
		"xoxb-", "xoxp-", "xapp-", "nvapi-", "hf_", "AIza", "ya29.",
		NULL
	};
	size_t				i;
	size_t				len;

	i = 0;
	while (heads[i] != NULL)
	{
		len = strlen(heads[i]);
		if (strncmp(at, heads[i], len) == 0 && token_len(at + len) >= 8)
			return (len + token_len(at + len));
		i++;
	}
	return (0);
}

/* Bulut erisim anahtari bicimi: dort harf oneki ve on alti buyuk harf. */
static size_t	match_cloud_key(const char *at)
{
	size_t	n;

	if (strncmp(at, "AKIA", 4) != 0 && strncmp(at, "ASIA", 4) != 0)
		return (0);
	n = 4;
	while (at[n] != '\0' && (isupper((unsigned char)at[n])
			|| isdigit((unsigned char)at[n])))
		n++;
	if (n < 20)
		return (0);
	return (n);
}

/* Imzali jeton: noktayla ayrilmis uc base64 parca, ilki "eyJ" ile baslar. */
static size_t	match_signed_token(const char *at)
{
	size_t	n;
	int		dots;

	if (strncmp(at, "eyJ", 3) != 0)
		return (0);
	n = 0;
	dots = 0;
	while (at[n] != '\0' && (is_b64ish(at[n]) || at[n] == '.'))
	{
		if (at[n] == '.')
			dots++;
		n++;
	}
	if (dots < 2 || n < 40)
		return (0);
	return (n);
}

/* Ozel anahtar blogu: baslik satirindan bitis satirinin sonuna kadar. */
static size_t	match_private_block(const char *at)
{
	const char	*end;

	if (strncmp(at, "-----BEGIN", 10) != 0)
		return (0);
	end = strstr(at, "-----END");
	if (end == NULL)
		return (strlen(at));
	end = strstr(end + 8, "-----");
	if (end == NULL)
		return (strlen(at));
	return ((size_t)(end - at) + 5);
}

/*
** Adres icine gomulmus kullanici adi ve parola.
**
** Yalnizca kimlik bolumu maskeleniyor, adresin tamami degil: SEMA, makine
** adi ve yol baglam icin degerli ve sir degil. Ilk yazim semayi da
** maskeliyordu; "https://" bilgisini kaybetmek modelin komutu
** anlamasini zorlastirirdi.
**
** Esleme GERIYE bakarak yapiliyor: konum "://" den hemen sonra mi. Ileriye
** bakip semayi de yutmak daha kolaydi ama yanlis bolumu maskeliyordu.
*/
static size_t	match_url_credential(const char *text, const char *at)
{
	size_t	n;
	int		colon;

	if (at - text < 3 || strncmp(at - 3, "://", 3) != 0)
		return (0);
	n = 0;
	colon = 0;
	while (at[n] != '\0' && at[n] != '@' && at[n] != '/' && at[n] != ' ')
	{
		if (at[n] == ':')
			colon = 1;
		n++;
	}
	if (at[n] != '@' || colon == 0)
		return (0);
	return (n);
}

/* Dizinin tamami verilen sinifa uyuyor mu. */
static int	all_match(const char *at, size_t n, int (*ok)(char))
{
	size_t	i;

	i = 0;
	while (i < n)
	{
		if (ok(at[i]) == 0)
			return (0);
		i++;
	}
	return (1);
}

/* Karakter rakam mi. */
static int	is_digit_char(char c)
{
	return (isdigit((unsigned char)c) != 0);
}

/* Karakter kucuk harf mi. */
static int	is_lower_char(char c)
{
	return (c >= 'a' && c <= 'z');
}

/*
** Uzun onaltilik ya da base64 gorunumlu dizi.
**
** NEGATIF KONTROLLER ONCE GELIR, sira onemli: rakamlar ayni zamanda
** onaltilik basamak oldugu icin "tamami onaltilik mi" denetimi tamami
** rakamdan olusan bir sayiyi da yakalardi. Ilk yazim boyleydi ve otuz
** alti haneli bir sayi sir sanildi.
**
** TAMAMI RAKAM SIR SAYILMAZ: zaman damgasi, boyut ve kimlik numarasi
** gibi seyler sik gecer ve maskelenmeleri baglami bozar.
**
** TAMAMI KUCUK HARF OLAN DIZI, ONALTILIK DEGILSE sayilmaz: duz yazi uzun
** olabilir. Kosulun ikinci yarisi sart - "abcdefabcdef..." hem kucuk harf
** hem onaltilik ve o bir sir. Yalnizca "kucuk harfse gec" demek o sinifi
** acikta birakirdi.
*/
static size_t	match_long_run(const char *at)
{
	size_t	n;

	n = token_len(at);
	if (n <= REDACT_LONG_RUN)
		return (0);
	if (all_match(at, n, is_digit_char))
		return (0);
	if (all_match(at, n, is_lower_char) && all_match(at, n, is_hex) == 0)
		return (0);
	return (n);
}

/*
** Uzun secenek biciminde verilen sir.
**
** "--password=DEGER" ve "--token DEGER" iki bicim de taraniyor. Yalnizca
** DEGER maskeleniyor; secenek adi baglam icin gerekli, cunku modelin
** komutun yapisini gormesi lazim.
*/
static size_t	match_option_value(const char *at, size_t *skip)
{
	static const char	*const	names[] = {
		"--password", "--passwd", "--pass", "--token", "--api-key",
		"--apikey", "--secret", "--access-token", "--auth", NULL
	};
	size_t				i;
	size_t				len;

	i = 0;
	while (names[i] != NULL)
	{
		len = strlen(names[i]);
		if (strncmp(at, names[i], len) == 0
			&& (at[len] == '=' || at[len] == ' '))
		{
			*skip = len + 1;
			return (strcspn(at + len + 1, " \t"));
		}
		i++;
	}
	return (0);
}

/* Konumdaki kuralli eslesmeyi bulur; uzunlugu ve etiketi verir. */
static size_t	match_any(const char *text, const char *at,
		const char **label, size_t *skip)
{
	size_t	n;

	*skip = 0;
	n = match_private_block(at);
	if (n > 0)
		return (*label = "ozel-anahtar", n);
	n = match_url_credential(text, at);
	if (n > 0)
		return (*label = "url-kimlik", n);
	n = match_option_value(at, skip);
	if (n > 0)
		return (*label = "parola", n);
	if (at_word_start(text, at) == 0)
		return (0);
	n = match_signed_token(at);
	if (n > 0)
		return (*label = "imzali-jeton", n);
	n = match_cloud_key(at);
	if (n > 0)
		return (*label = "bulut-anahtar", n);
	n = match_prefix(at);
	if (n > 0)
		return (*label = "anahtar", n);
	n = match_long_run(at);
	if (n > 0)
		return (*label = "uzun-dizi", n);
	return (0);
}

/* Yer tutucuyu tampona yazar. */
static int	push_mark(t_buf *buf, const char *label)
{
	if (buf_push_str(buf, "[GIZLI:") == 0)
		return (0);
	if (buf_push_str(buf, label) == 0)
		return (0);
	return (buf_push(buf, ']'));
}

/*
** Metindeki sirlari yer tutucuyla degistirir; cagiran serbest birakir.
**
** NULL girdi bos dize verir: cagiranin her yerde NULL denetimi yapmak
** zorunda kalmamasi, sessizce atlanan bir alan riskinden iyidir.
*/
char	*redact_text(const char *text)
{
	t_buf		buf;
	const char	*label;
	size_t		n;
	size_t		skip;
	size_t		i;

	buf_init(&buf);
	if (text == NULL)
		return (strdup(""));
	i = 0;
	while (text[i] != '\0')
	{
		n = match_any(text, text + i, &label, &skip);
		if (n == 0)
		{
			if (buf_push(&buf, text[i]) == 0)
				return (buf_free(&buf), NULL);
			i++;
			continue ;
		}
		while (skip > 0)
		{
			if (buf_push(&buf, text[i]) == 0)
				return (buf_free(&buf), NULL);
			i++;
			skip--;
		}
		if (push_mark(&buf, label) == 0)
			return (buf_free(&buf), NULL);
		i += n;
	}
	return (buf_take(&buf));
}

/*
** Degiskenin ADINDA sir gecip gecmedigini soyler.
**
** Ortam degiskenlerinin yalnizca ISIMLERI gonderiliyor; bu islev ismin
** kendisinin bile gonderilmemesi gereken durumlar icin degil, DEGERININ
** hicbir kosulda gonderilmemesi gereken degiskenleri ayirmak icin var.
**
** Esleme parca bazli ve buyuk/kucuk harf duyarsiz: "MY_github_TOKEN" de
** "AWS_ACCESS_KEY_ID" de yakalanir. Tam ad listesi tutmak her yeni
** servis icin guncelleme gerektirirdi; parca listesi kendiliginden
** genisliyor.
*/
int	redact_is_secret_name(const char *name)
{
	static const char	*const	marks[] = {
		"KEY", "TOKEN", "SECRET", "PASS", "CRED", "AUTH", "PRIVATE",
		"COOKIE", "SESSION", "BEARER", "AWS", "STRIPE", "DATABASE_URL",
		NULL
	};
	char				upper[256];
	size_t				i;

	if (name == NULL)
		return (0);
	i = 0;
	while (name[i] != '\0' && i + 1 < sizeof(upper))
	{
		upper[i] = (char)toupper((unsigned char)name[i]);
		i++;
	}
	upper[i] = '\0';
	i = 0;
	while (marks[i] != NULL)
	{
		if (strstr(upper, marks[i]) != NULL)
			return (1);
		i++;
	}
	return (0);
}
