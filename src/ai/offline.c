/*
** offline.c - elle yazilmis niyet tablosu.
**
** NEDEN VAR:
**   docs/CONFIG.md'nin tezi su: cevrimdisi degerin buyuk kismi dil
**   modelinden degil, zaten var olan belirlenimci katmandan geliyor.
**   Aga cikmayan ve bellek tutmayan bu katman zaten uc parcadan
**   olusuyordu - yazim duzeltmesi, PATH cozumlemesi, siniflandirici - ve
**   bu dosya dordunculeri ekliyor: sik kullanilan niyetlerin dogrudan
**   karsiligi.
**
** TABLO MODELDEN ONCE BAKILIYOR:
**   Eslesme bulunursa yardimci surec hic baslatilmiyor. Sonuc sifir
**   gecikme, sifir ucret ve her seferinde AYNI komut. Bir kabukta
**   "disk kullanimini goster" satirinin cevabi tartismali degil; onu bir
**   ag turuna baglamak kazanc getirmiyor.
**
** ESLESME ANAHTAR SOZCUK KUMESIYLE, TAM METINLE DEGIL:
**   Tam metin eslesmesi kirilgan olurdu: "disk kullanimi" ile "disk
**   kullanimini goster" ayni istek ama farkli metin. Bunun yerine her
**   kaydin anahtar sozcukleri var ve HEPSI satirda geciyorsa kayit
**   eslesiyor. Birden fazla kayit eslesirse en cok anahtar sozcugu olan
**   kazaniyor; esitlikte tablodaki ilk kayit.
**
** KAYIT SATIRIN TAMAMINI KARSILAMAK ZORUNDA:
**   Anahtar sozcuklerin hepsinin satirda gecmesi YETMEZ; satirda kaydin
**   hesaplayamadigi bir sozcuk de kalmamalidir. Gercek bir kullanimda
**   olculdu: "dun degisen dosyalari zip'le" satirinda "dun degisen dosya"
**   kaydi uc anahtar sozcukle eslesti ve kullaniciya find komutu onerildi.
**   Oysa istenen is zip'lemekti; kayit satirin yarisini aciklayip kalanini
**   sessizce yok saydi.
**
**   Dolgu sozcukleri ("goster", "listele", "ne kadar") bu denetimden muaf:
**   hangi komutun dogru oldugunu degistirmiyorlar. "zip", "sil", "yedekle"
**   degistiriyor. Boyle bir sozcuk artakalirsa tablo SUSUYOR ve satir
**   modele gidiyor. Susmanin bedeli bir ag turu, yanlis komut onermenin
**   bedeli kullanicinin verisi.
**
** EN AZ IKI ANAHTAR SOZCUK SART:
**   Tek sozcukle eslesmek yanlis pozitif uretir - "disk" sozcugu baska
**   bir istekte de gecebilir. Iki sozcuk sarti tabloyu temkinli tutuyor
**   ve temkinli olmasi gerekiyor: yanlis bir eslesme kullaniciya yanlis
**   komut onermek demek, oysa eslesmemenin bedeli yalnizca bir ag turu.
**
** TURKCE KARAKTERLER ASCII'YE KATLANIYOR:
**   Kullanici ASCII ile de Turkce harflerle de yazabilir. Karsilastirma
**   oncesi katlama, tabloyu iki kez yazmaktan iyi.
*/

#include "nax.h"
#include "ai.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define OFFLINE_MIN_WORDS 2
#define OFFLINE_SUFFIX_MIN 4

/*
** Niyet tablosu.
**
** Her satir: anahtar sozcukler (bosluklu), karsiligi olan komut. Kayitlar
** kasten az ve tartismasiz: bir komutun dogrulugu tartismaliysa tabloya
** girmemeli, modele gitmeli.
**
** AYNI KOMUT ICIN BIRDEN FAZLA SATIR OLABILIR. Turkce ekler tek bir kokle
** kapsanamiyor: "dosya sayisi" ile "kac dosya" ayni isi istiyor ama ortak
** anahtar sozcugu yok. Govdeye bir cekim cozucu koymak yerine satir
** eklemek hem basit hem ongorulebilir - tablo zaten elle yazilmis bir
** liste, akilli olmasi beklenmiyor.
*/
static const char	*const	g_table[][2] = {
	{"disk kullanim", "df -h"},
	{"disk doluluk", "df -h"},
	{"bos alan", "df -h"},
	{"buyuk dosya", "du -ah . | sort -rh | head -20"},
	{"dizin boyut", "du -sh ."},
	{"klasor boyut", "du -sh ."},
	{"git durum", "git status"},
	{"git dal liste", "git branch -a"},
	{"git dallari", "git branch -a"},
	{"son commit", "git log --oneline -10"},
	{"degisiklik liste", "git status --short"},
	{"calisan surec", "ps aux"},
	{"surec liste", "ps aux"},
	{"bellek kullanim", "free -h"},
	{"dinlenen port", "ss -tlnp"},
	{"acik port", "ss -tlnp"},
	{"ag baglanti", "ss -tunap"},
	{"dun degisen dosya", "find . -newermt \"1 day ago\" -type f"},
	{"bugun degisen dosya", "find . -newermt \"today\" -type f"},
	{"bos dizin bul", "find . -type d -empty"},
	{"dosya sayi", "find . -type f | wc -l"},
	{"kac dosya", "find . -type f | wc -l"},
	{"gizli dosya liste", "ls -la"},
	{"sistem bilgi", "uname -a"},
	{"tarih saat", "date"},
	{NULL, NULL}
};

/* Turkce harfi ASCII karsiligina katlar; tanimadigini oldugu gibi verir. */
static const char	*fold_one(const char *at, size_t *used)
{
	static const char	*const	pairs[] = {
		"\xc3\xa7", "c", "\xc3\x87", "c", "\xc4\x9f", "g", "\xc4\x9e", "g",
		"\xc4\xb1", "i", "\xc4\xb0", "i", "\xc3\xb6", "o", "\xc3\x96", "o",
		"\xc5\x9f", "s", "\xc5\x9e", "s", "\xc3\xbc", "u", "\xc3\x9c", "u",
		NULL, NULL
	};
	size_t				i;

	i = 0;
	while (pairs[i] != NULL)
	{
		if (strncmp(at, pairs[i], 2) == 0)
		{
			*used = 2;
			return (pairs[i + 1]);
		}
		i += 2;
	}
	*used = 1;
	return (NULL);
}

/*
** Metni kucuk harfe cevirip Turkce harfleri katlar; cagiran birakir.
**
** Harf olmayan her sey bosluga cevriliyor: noktalama ve soru isaretleri
** eslesmeyi bozmasin. "disk kullanimi?" ile "disk kullanimi" ayni istek.
*/
static char	*fold_text(const char *text)
{
	t_buf		buf;
	const char	*folded;
	size_t		used;

	buf_init(&buf);
	while (*text != '\0')
	{
		folded = fold_one(text, &used);
		if (folded != NULL)
		{
			if (buf_push_str(&buf, folded) == 0)
				return (buf_free(&buf), NULL);
		}
		else if (isalnum((unsigned char)*text))
		{
			if (buf_push(&buf, (char)tolower((unsigned char)*text)) == 0)
				return (buf_free(&buf), NULL);
		}
		else if (buf_push(&buf, ' ') == 0)
			return (buf_free(&buf), NULL);
		text += used;
	}
	if (buf.data == NULL)
		return (strdup(""));
	return (buf_take(&buf));
}

/*
** Sozcuk katlanmis metinde TAM sozcuk olarak geciyor mu.
**
** Parca eslesmesi yanlis pozitif uretirdi: "dal" sozcugu "dalga" icinde
** de geciyor. Katlanmis metinde her sozcuk bosluklarla cevrili oldugu
** icin sinir denetimi iki karaktere bakmakla bitiyor.
**
** ONEK ESLESMESINE IZIN VAR: tablodaki "kullanim" sozcugu "kullanimi" ve
** "kullanimini" ile de eslesiyor. Turkce eklerin tamamini yazmak tabloyu
** okunmaz hale getirirdi.
**
** AMA YALNIZCA DORT HARFTEN UZUN SOZCUKLER ICIN. Olculdu: kisa
** sozcuklerde ek toleransi yanlis pozitif uretiyor - "say" sozcugu
** "sayfa" icinde geciyor ve "kac sayfa dosya var" satiri dosya sayma
** komutu sanildi. Kisa sozcukler tam sinir istiyor.
*/
/* Sozcugun ardindan gelen karakter ek olarak kabul edilebilir mi. */
static int	suffix_ok(const char *word, char next)
{
	if (next == '\0' || next == ' ')
		return (1);
	if (strlen(word) < OFFLINE_SUFFIX_MIN)
		return (0);
	return (islower((unsigned char)next) != 0);
}

/* Sozcuk katlanmis metinde tam sozcuk ya da ekli hali olarak geciyor mu. */
static int	has_word(const char *folded, const char *word)
{
	size_t		len;
	const char	*at;

	len = strlen(word);
	at = folded;
	while (1)
	{
		at = strstr(at, word);
		if (at == NULL)
			return (0);
		if ((at == folded || at[-1] == ' ') && suffix_ok(word, at[len]))
			return (1);
		at++;
	}
}

/* Kaydin tum anahtar sozcukleri metinde geciyorsa sozcuk sayisini verir. */
static int	score_entry(const char *folded, const char *keys)
{
	char	*copy;
	char	*word;
	int		count;

	copy = strdup(keys);
	if (copy == NULL)
		return (0);
	count = 0;
	word = strtok(copy, " ");
	while (word != NULL)
	{
		if (has_word(folded, word) == 0)
			return (free(copy), 0);
		count++;
		word = strtok(NULL, " ");
	}
	free(copy);
	if (count < OFFLINE_MIN_WORDS)
		return (0);
	return (count);
}

/*
** Satirdaki sozcuk, hangi komutun dogru oldugunu degistirmeyen bir dolgu mu.
**
** Liste kasten kisa ve yalnizca "bana goster" anlamindaki sozcuklerden
** olusuyor. Bir is BILDIREN sozcuk (zip, sil, yedekle, tasi) buraya asla
** girmez; girerse tablonun susma guvencesi bozulur.
*/
static int	is_filler(const char *word, size_t len)
{
	static const char	*const	fillers[] = {
		"goster", "gor", "gormek", "listele", "soyle", "bul", "bahset",
		"yaz", "ver", "bak", "bana", "lutfen", "istiyorum", "ister",
		"ne", "kadar", "var", "mi", "mu", "bir", "tum", "tumu", "butun",
		"hepsi", "su", "bu", "burada", "buradaki", "icin", "ile", "de",
		"da", "ki", "en", "cok", "simdi", "anda", "an", "acaba",
		"sadece", "yalnizca", NULL
	};
	size_t						i;

	i = 0;
	while (fillers[i] != NULL)
	{
		if (strlen(fillers[i]) == len
			&& strncmp(fillers[i], word, len) == 0)
			return (1);
		i++;
	}
	return (0);
}

/*
** Sozcuk, kaydin anahtar sozcuklerinden biriyle karsilaniyor mu.
**
** has_word'un tersi yonu: orada tablodaki sozcuk satirda araniyor, burada
** satirdaki sozcugu tablodaki bir sozcuk aciklayabiliyor mu diye bakiliyor.
** Ek toleransi ayni kurala uyuyor, yoksa "dosyalari" sozcugu "dosya"
** kaydiyla eslesir ama onun tarafindan aciklanmis sayilmazdi.
*/
static int	key_covers(const char *keys, const char *word, size_t len)
{
	size_t	klen;
	size_t	i;

	i = 0;
	while (keys[i] != '\0')
	{
		klen = 0;
		while (keys[i + klen] != '\0' && keys[i + klen] != ' ')
			klen++;
		if (klen <= len && strncmp(keys + i, word, klen) == 0
			&& (klen == len || (klen >= OFFLINE_SUFFIX_MIN
					&& islower((unsigned char)word[klen]))))
			return (1);
		i += klen;
		while (keys[i] == ' ')
			i++;
	}
	return (0);
}

/* Satirin her sozcugu bu kayit ya da dolgu listesiyle karsilaniyor mu. */
static int	line_covered(const char *folded, const char *keys)
{
	size_t	i;
	size_t	len;

	i = 0;
	while (folded[i] != '\0')
	{
		while (folded[i] == ' ')
			i++;
		len = 0;
		while (folded[i + len] != '\0' && folded[i + len] != ' ')
			len++;
		if (len > 0 && is_filler(folded + i, len) == 0
			&& key_covers(keys, folded + i, len) == 0)
			return (0);
		i += len;
	}
	return (1);
}

/*
** Satirin tablodaki karsiligini verir; yoksa NULL.
**
** Donen metin tablonun kendisine ait, sabit; cagiran serbest BIRAKMAZ.
*/
const char	*offline_lookup(const char *text)
{
	char		*folded;
	const char	*best;
	int			best_score;
	int			score;
	size_t		i;

	folded = fold_text(text);
	if (folded == NULL)
		return (NULL);
	best = NULL;
	best_score = 0;
	i = 0;
	while (g_table[i][0] != NULL)
	{
		score = score_entry(folded, g_table[i][0]);
		if (score > best_score && line_covered(folded, g_table[i][0]))
		{
			best_score = score;
			best = g_table[i][1];
		}
		i++;
	}
	free(folded);
	return (best);
}
