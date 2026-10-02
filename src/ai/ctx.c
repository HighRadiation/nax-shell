/*
** ctx.c - oturum baglami: AI'in sormadan bildigi seyler.
**
** PROJEYI AYIRT EDEN IKINCI SEY BURASI. Icerideki AI calisma dizinini,
** son komutlari, cikis kodlarini ve git dalini SORMADAN bilir. Bir
** terminalde ayri bir program olarak kosan yardimci bunlari bilemez;
** kabugun icinde olmanin asil kazanci bu.
**
** MASKELEME GIRISTE YAPILIR, CIKISTA DEGIL:
**   Komut halkaya girerken temizleniyor. Iki sebebi var. Birincisi "nax
**   ctx" sozlesmesi: o komut gidecek baytlari AYNEN basmak zorunda ve
**   maskelemeyi gonderme anina birakmak iki ayri yol uretirdi. Ikincisi
**   daha onemli - temizlenmemis bir sir bellekte hic durmaz.
**
** BOSLUKLA BASLAYAN SATIR HIC GIRMEZ:
**   Kabuklarda bir bosluk ile baslayan satirin gecmise yazilmamasi yaygin
**   bir aliskanliktir. Burada ayni anlami tasiyor ve bedava geliyor:
**   kullanicinin zaten bildigi bir hareket, baglamdan kacmanin yolu
**   oluyor.
**
** SIRA ONEMLI, ESKIDEN YENIYE:
**   Model son komuta en cok agirlik vermeli ve dil modelleri sonda olana
**   daha cok dikkat eder. Halkadan okuma bu yuzden en eskiden baslayip
**   en yeniyle bitiyor.
*/

#include "nax.h"
#include "ai.h"
#include <stdlib.h>
#include <string.h>
#include "exec.h"
#include <stdio.h>

static t_ctx	g_ctx;

/* Halkadaki tum kayitlari birakir. */
void	ctx_clear(void)
{
	size_t	i;

	i = 0;
	while (i < CTX_RING)
	{
		free(g_ctx.ring[i].cmd);
		g_ctx.ring[i].cmd = NULL;
		i++;
	}
	g_ctx.next = 0;
	g_ctx.filled = 0;
}

/*
** Komutu halkaya ekler; bosluk ile baslayan satir yok sayilir.
**
** Metin maskelenerek saklaniyor. Maskeleme basarisiz olursa kayit hic
** eklenmiyor: temizlenmemis bir satiri saklamak, kaydi kaybetmekten cok
** daha kotu.
*/
void	ctx_add(const char *line, int code)
{
	char	*clean;

	if (line == NULL || *line == ' ' || *line == '\t')
		return ;
	while (*line == ' ' || *line == '\t')
		line++;
	if (*line == '\0')
		return ;
	clean = redact_text(line);
	if (clean == NULL)
		return ;
	free(g_ctx.ring[g_ctx.next].cmd);
	g_ctx.ring[g_ctx.next].cmd = clean;
	g_ctx.ring[g_ctx.next].code = code;
	g_ctx.next = (g_ctx.next + 1) % CTX_RING;
	if (g_ctx.filled < CTX_RING)
		g_ctx.filled++;
}

/* Halkadaki sirali konumun gercek dizinini verir; 0 en eski. */
static size_t	ring_index(size_t order)
{
	size_t	start;

	if (g_ctx.filled < CTX_RING)
		start = 0;
	else
		start = g_ctx.next;
	return ((start + order) % CTX_RING);
}

/* Son komutlari eskiden yeniye tampona yazar. */
static int	push_recent(t_buf *buf)
{
	char	line[64];
	size_t	order;
	size_t	at;

	if (g_ctx.filled == 0)
		return (1);
	if (buf_push_str(buf, "son komutlar:\n") == 0)
		return (0);
	order = 0;
	while (order < g_ctx.filled)
	{
		at = ring_index(order);
		snprintf(line, sizeof(line), "  [%d] ", g_ctx.ring[at].code);
		if (buf_push_str(buf, line) == 0)
			return (0);
		if (buf_push_str(buf, g_ctx.ring[at].cmd) == 0)
			return (0);
		if (buf_push(buf, '\n') == 0)
			return (0);
		order++;
	}
	return (1);
}

/*
** Isteklerle gonderilecek baglam blogunu kurar; cagiran birakir.
**
** Blok tek bir alanda gidiyor, cunku protokol alan sayisini on alti ile
** sinirliyor ve baglamin parca sayisi zamanla degisecek. Tek alan, bicimi
** degistirmeden buyumeye izin veriyor.
*/
char	*ctx_block(void)
{
	t_buf	buf;

	buf_init(&buf);
	if (ctx_push_facts(&buf) == 0 || push_recent(&buf) == 0)
	{
		buf_free(&buf);
		return (NULL);
	}
	return (buf_take(&buf));
}

/*
** "nax ctx" komutunun bastigi metin: bir sonraki istekte gidecek alanlar.
**
** GIZLILIK IDDIASINI DENETLENEBILIR KILAN TEK OZELLIK BU. Belgeye
** guvenmek zorunda degilsin, bakabilirsin. Kullanicinin yazacagi satir
** dogal olarak burada yok; onun disindaki her sey aynen gosteriliyor.
**
** IKI BOLUM AYRI ETIKETLI: olgular ve son komutlar her istekte gidiyor,
** ortam degiskeni adlari yalnizca oturum acilisinda gitti. Ilk yazim
** ikinciyi hic gostermiyordu ve bu, gonderilenin en buyuk parcasini
** denetlenemez birakiyordu - testi yakaladi.
*/
char	*ctx_dump(void)
{
	t_buf	buf;
	char	*block;
	char	*names;
	int		ok;

	block = ctx_block();
	names = ctx_env_names();
	if (block == NULL || names == NULL)
		return (free(block), free(names), NULL);
	buf_init(&buf);
	ok = buf_push_str(&buf, "--- her istekte gidiyor ---\n");
	ok = ok && buf_push_str(&buf, block);
	ok = ok && buf_push_str(&buf, "--- yalniz oturum acilisinda gitti ---\n");
	ok = ok && buf_push_str(&buf, names);
	ok = ok && buf_push_str(&buf, "--- bunun disinda hicbir sey ---\n");
	free(block);
	free(names);
	if (ok == 0)
		return (buf_free(&buf), NULL);
	return (buf_take(&buf));
}

/*
** "ctx" yerlesigi: bir sonraki istekte gidecek baytlari basar.
**
** NEDEN YERLESIK: harici bir program kabugun bellegindeki halkayi
** goremez. Gizlilik iddiasini denetlenebilir kilan tek ozellik bu, o
** yuzden kabugun kendi icinde olmak zorunda.
**
** CIKTI STDOUT'A GIDIYOR: kullanici bunu bir dosyaya yonlendirip
** inceleyebilsin, hata ciktisina karismasin.
*/
int	bi_ctx(t_shell *sh, char **argv)
{
	char	*text;

	(void)argv;
	text = ctx_dump();
	if (text == NULL)
	{
		ex_warn("baglam uretilemedi");
		return (1);
	}
	printf("%s", text);
	free(text);
	(void)sh;
	return (0);
}
