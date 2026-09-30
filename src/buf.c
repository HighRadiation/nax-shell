/*
** buf.c — buyuyebilen metin tamponu.
**
** NEDEN AYRI MODUL:
**   Sozcuk ayirici parcalari, kanonik yazdirici ciktiyi, genisletme ise
**   degisken degerlerini karakter karakter biriktirmek zorunda. Ucu de
**   ayni seye ihtiyac duydugu icin tampon tek bir yerde durur; aksi halde
**   ayni on satir uc dosyada tekrarlanirdi.
**
** SAHIPLIK:
**   buf_take tampondaki metnin sahipligini CAGIRANA devreder ve tamponu
**   bosaltir. Devredilmemis bir tampon buf_free ile birakilir. Ikisinden
**   biri her yolda cagrilmak zorunda; hata yollari dahil.
**
** HATA BILDIRIMI:
**   Ekleme fonksiyonlari basarida 1, bellek ayrilamadiginda 0 doner.
**   Cagiran bu degeri yok saymamali; lexer bunu hata yoluna baglar.
*/

#include "nax.h"
#include <stdlib.h>
#include <string.h>

#define BUF_START 32

/* Tamponu bos duruma kurar; kullanim oncesi bir kez cagrilir. */
void	buf_init(t_buf *buf)
{
	buf->data = NULL;
	buf->len = 0;
	buf->cap = 0;
}

/* Tampona bir karakter ekler; yer yoksa buyutur. Basarisizlikta 0 doner. */
int	buf_push(t_buf *buf, char c)
{
	char	*bigger;
	size_t	want;

	if (buf->len + 1 >= buf->cap)
	{
		want = buf->cap * 2;
		if (want < BUF_START)
			want = BUF_START;
		bigger = realloc(buf->data, want);
		if (bigger == NULL)
			return (0);
		buf->data = bigger;
		buf->cap = want;
	}
	buf->data[buf->len] = c;
	buf->len++;
	buf->data[buf->len] = '\0';
	return (1);
}

/* Tampona bir metni ekler; basarisizlikta 0 doner. */
int	buf_push_str(t_buf *buf, const char *s)
{
	while (*s != '\0')
	{
		if (buf_push(buf, *s) == 0)
			return (0);
		s++;
	}
	return (1);
}

/* Tampon bos mu; hic yazilmamis ya da alinmis olabilir. */
int	buf_empty(const t_buf *buf)
{
	return (buf->data == NULL || buf->len == 0);
}

/* Metnin sahipligini cagirana devreder ve tamponu bosaltir. */
char	*buf_take(t_buf *buf)
{
	char	*taken;

	if (buf->data == NULL)
		return (strdup(""));
	taken = buf->data;
	buf_init(buf);
	return (taken);
}

/* Devredilmemis tamponu birakir; birden fazla cagrilmasi guvenlidir. */
void	buf_free(t_buf *buf)
{
	free(buf->data);
	buf_init(buf);
}
