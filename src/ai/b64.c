/*
** b64.c - tel bicimindeki serbest metin alanlari icin base64.
**
** NEDEN GEREKLI:
**   Kayitlar satir tabanli ve sekme ile ayrilmis. Kullanicinin yazdigi
**   metin sekme ya da yenisatir icerdiginde cerceveyi bozar; kodlamak bu
**   ihtimali tamamen ortadan kaldiriyor. Alternatifi kacis dizileri
**   olurdu ama o, iki tarafta da ayri bir kacis cozucu demek.
**
** NEDEN YERINDE COZULEBILIYOR:
**   Dort kodlanmis karakter uc bayta cozulur, yani yazma indisi her zaman
**   okuma indisinin GERISINDE kalir. Bu yuzden ayri bir cikti tamponu
**   gerekmiyor ve cozulen degerler cercevenin kendi tamponunda yasiyor.
**
** DOGRULAMA KATI:
**   Uzunluk dordun kati olmali, dolgu yalnizca sonda ve en fazla iki
**   karakter. Gevsek bir cozucu bozuk cerceveyi sessizce kabul eder;
**   oysa bozuk cerceve yardimci surecin saglikli olmadiginin isareti ve
**   gorulmesi gerekiyor.
**
**   Uzunluk kontrolu TESTLE GOZLEMLENEMIYOR: dordun kati olmayan bir
**   dizgide son dortlu NUL sonlandiricisini okur, o da alfabede
**   olmadigi icin cozme yine basarisiz olur. Kontrol yine duruyor,
**   cunku bicim kuralini okuyanin aradigi yerde soyluyor ve dogrulugu
**   "sonlandirici bizi kurtarir" akil yurutmesine baglamiyor.
**   FINDINGS.md icinde kayitli.
*/

#include "proto.h"
#include <stdlib.h>

/* Kodlama alfabesi. */
static const char	*b64_chars(void)
{
	return ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		"0123456789+/");
}

/* Uc bayti dort karaktere cevirir. */
static void	b64_triplet(const unsigned char *in, char *out)
{
	const char	*set;
	unsigned long	v;

	set = b64_chars();
	v = ((unsigned long)in[0] << 16) | ((unsigned long)in[1] << 8) | in[2];
	out[0] = set[(v >> 18) & 0x3f];
	out[1] = set[(v >> 12) & 0x3f];
	out[2] = set[(v >> 6) & 0x3f];
	out[3] = set[v & 0x3f];
}

/* Kalan bir ya da iki bayti dolgu ile dort karaktere cevirir. */
static void	b64_tail(const unsigned char *in, size_t left, char *out)
{
	const char	*set;
	unsigned long	v;

	set = b64_chars();
	v = (unsigned long)in[0] << 16;
	if (left == 2)
		v |= (unsigned long)in[1] << 8;
	out[0] = set[(v >> 18) & 0x3f];
	out[1] = set[(v >> 12) & 0x3f];
	out[2] = '=';
	out[3] = '=';
	if (left == 2)
		out[2] = set[(v >> 6) & 0x3f];
}

/* Veriyi kodlar; cagiran serbest birakir. Bos girdi bos dize verir. */
char	*b64_encode(const unsigned char *in, size_t n)
{
	char	*out;
	size_t	i;
	size_t	j;

	out = malloc((n + 2) / 3 * 4 + 1);
	if (out == NULL)
		return (NULL);
	i = 0;
	j = 0;
	while (n >= 3 && i + 3 <= n)
	{
		b64_triplet(in + i, out + j);
		i += 3;
		j += 4;
	}
	if (i < n)
	{
		b64_tail(in + i, n - i, out + j);
		j += 4;
	}
	out[j] = '\0';
	return (out);
}

/* Tek karakterin degerini verir; alfabede yoksa -1. */
static int	b64_value(char c)
{
	const char	*set;
	int			i;

	set = b64_chars();
	i = 0;
	while (set[i] != '\0')
	{
		if (set[i] == c)
			return (i);
		i++;
	}
	return (-1);
}

/*
** Dort karakteri cozup yazdigi bayt sayisini dondurur; bozukta -1.
**
** Dolgu yalnizca son iki konumda olabilir ve ucuncu konum dolguysa
** dorduncu de dolgu olmak zorunda; aksi halde kodlama gecersiz.
*/
static int	b64_quad(const char *in, char *out)
{
	int				v[4];
	int				i;
	unsigned long	bits;

	i = 0;
	while (i < 4)
	{
		if (in[i] == '=')
			v[i] = -2;
		else
			v[i] = b64_value(in[i]);
		if (v[i] == -1)
			return (-1);
		i++;
	}
	if (v[0] == -2 || v[1] == -2 || (v[2] == -2 && v[3] != -2))
		return (-1);
	bits = ((unsigned long)v[0] << 18) | ((unsigned long)v[1] << 12);
	if (v[2] != -2)
		bits |= (unsigned long)v[2] << 6;
	if (v[3] != -2)
		bits |= (unsigned long)v[3];
	out[0] = (char)((bits >> 16) & 0xff);
	out[1] = (char)((bits >> 8) & 0xff);
	out[2] = (char)(bits & 0xff);
	if (v[2] == -2)
		return (1);
	if (v[3] == -2)
		return (2);
	return (3);
}

/*
** Metni YERINDE cozer; basarida 1, bozuk kodlamada 0 doner.
**
** Cozulen uzunluk *out_len'e yazilir. Metin ayrica NUL ile kapatilir, ama
** uzunluk da veriliyor: cozulen veri icinde NUL bayti varsa cagiran bunu
** uzunluk ile dizgi uzunlugunu karsilastirarak gorebilsin diye. Tel
** bicimindeki tum alanlar metin oldugu icin gomulu NUL bir ihlaldir.
*/
int	b64_decode_inplace(char *text, size_t *out_len)
{
	size_t	r;
	size_t	w;
	size_t	len;
	int		got;

	len = 0;
	while (text[len] != '\0')
		len++;
	if (len % 4 != 0)
		return (0);
	r = 0;
	w = 0;
	while (r < len)
	{
		got = b64_quad(text + r, text + w);
		if (got < 0)
			return (0);
		if (r + 4 < len && got != 3)
			return (0);
		r += 4;
		w += (size_t)got;
	}
	text[w] = '\0';
	*out_len = w;
	return (1);
}
