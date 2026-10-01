/*
** lineio.c - akistan tam kayit satirlari toplamak.
**
** NEDEN OKUMA ILE AYIRMA AYRI:
**   rd_feed akistan okur, rd_take tamponda biriken veriden tam satirlari
**   verir. Tek islev olsa cagiran poll ile bekleyemezdi: once beklemek,
**   hazir oldugunda okumak ve sonra tamponu BOSALTANA kadar satir
**   cikarmak gerekiyor. Bir okuma birden fazla satir getirebilir ve
**   hicbirini kaybetmemek icin bosaltma dongusu sart.
**
** YARIM SATIR NORMALDIR:
**   Boru, satir siniri diye bir sey bilmiyor; bir okuma satirin ortasinda
**   bitebilir ya da uc satiri birden getirebilir. Tampon bu yuzden var.
**
** AKIS BITTIGINDE YARIM SATIR ATILIR:
**   Tamamlanma sansi olmayan bir kayit cozulemez. Saklamak, yardimci
**   surec yeniden dogdugunda eski bir yarim satirin yeni veriye
**   eklenmesi demek olurdu.
**
** ASIRI UZUN SATIR:
**   Sinir asildiginda tampon bosaltilir ve bir sonraki yenisatira kadar
**   gelen her sey atilir. Boylece bir bozuk kayit akisin tamamini
**   zehirlemiyor - satir tabanli olmanin asil kazanci bu kurtarma.
*/

#include "proto.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define RD_CHUNK 4096

/* Okuyucuyu kurar; tampon ilk okumada ayrilir. */
void	rd_init(t_reader *reader, int fd)
{
	reader->fd = fd;
	reader->data = NULL;
	reader->len = 0;
	reader->cap = 0;
	reader->pending = 0;
	reader->drop = 0;
	reader->ended = 0;
}

/* Tamponu birakir; iki kez cagrilmasi guvenli. */
void	rd_free(t_reader *reader)
{
	free(reader->data);
	reader->data = NULL;
	reader->len = 0;
	reader->cap = 0;
	reader->pending = 0;
}

/* Durumun okunabilir adini verir; testler ve gunluk icin. */
const char	*rd_state_name(t_readst state)
{
	static const char	*const	names[] = {
		"LINE", "MORE", "EOF", "TOO_LONG", "ERROR", NULL
	};
	size_t				i;

	i = 0;
	while (names[i] != NULL)
	{
		if (i == (size_t)state)
			return (names[i]);
		i++;
	}
	return ("ERROR");
}

/*
** Tamponda en az need bayt yer acar; basarida 1.
**
** Ustten sinir PROTO_MAX_LINE + 2: bir tam satir, yenisatiri ve
** sonlandiricisi. Daha buyugune ihtiyac yok, cunku siniri asan satir
** zaten atiliyor.
**
** ISTENEN BOYUT SINIRA KIRPILIR, HATA DONMEZ. Ilk yazim sinirin ustunu
** isteyince basarisiz oluyordu ve rd_feed bunu RD_ERROR'a ceviriyordu;
** yani asiri uzun satir "okuma hatasi" gibi gorunuyordu. Oysa dogru
** cevap RD_TOO_LONG ve o karari veren yer rd_no_newline. Sinir mantigi
** tek yerde durmali.
*/
static int	rd_grow(t_reader *reader, size_t need)
{
	size_t	want;
	char	*bigger;

	if (need > PROTO_MAX_LINE + 2)
		need = PROTO_MAX_LINE + 2;
	if (need <= reader->cap)
		return (1);
	want = reader->cap;
	if (want < RD_CHUNK)
		want = RD_CHUNK;
	while (want < need)
		want *= 2;
	if (want > PROTO_MAX_LINE + 2)
		want = PROTO_MAX_LINE + 2;
	bigger = realloc(reader->data, want);
	if (bigger == NULL)
		return (0);
	reader->data = bigger;
	reader->cap = want;
	return (1);
}

/* Tamponun basindan n bayt atar. */
static void	rd_consume(t_reader *reader, size_t n)
{
	if (n >= reader->len)
	{
		reader->len = 0;
		return ;
	}
	memmove(reader->data, reader->data + n, reader->len - n);
	reader->len -= n;
}

/* Yenisatir yoksa hangi durumun dondurulecegine karar verir. */
static t_readst	rd_no_newline(t_reader *reader)
{
	if (reader->len > PROTO_MAX_LINE)
	{
		reader->drop = 1;
		reader->len = 0;
		return (RD_TOO_LONG);
	}
	if (reader->ended)
	{
		reader->len = 0;
		return (RD_EOF);
	}
	return (RD_MORE);
}

/*
** Akistan bir kez okur; RD_MORE veri geldi, RD_EOF akis bitti.
**
** EINTR'de YENIDEN DENENIR: cagiran poll ile hazir oldugunu gormus
** oldugu icin okuma veri dondurecek; sinyal tam o anda geldiyse bu bir
** hata degil, yarida kesilmis bir sistem cagrisi. Iptal istegi ayri bir
** tanimlayicidan gozlendigi icin burada donup durmak riski yok.
*/
t_readst	rd_feed(t_reader *reader)
{
	ssize_t	got;
	size_t	room;

	if (reader->ended)
		return (RD_EOF);
	if (rd_grow(reader, reader->len + RD_CHUNK) == 0)
		return (RD_ERROR);
	room = reader->cap - reader->len;
	if (room == 0)
		return (rd_no_newline(reader));
	got = read(reader->fd, reader->data + reader->len, room);
	while (got < 0 && errno == EINTR)
		got = read(reader->fd, reader->data + reader->len, room);
	if (got < 0)
		return (RD_ERROR);
	if (got == 0)
	{
		reader->ended = 1;
		return (RD_EOF);
	}
	reader->len += (size_t)got;
	return (RD_MORE);
}

/* Atlama kipinde kalani gecer; kip bittiyse 1 doner. */
static int	rd_drain_drop(t_reader *reader)
{
	char	*nl;

	nl = memchr(reader->data, '\n', reader->len);
	if (nl == NULL)
	{
		reader->len = 0;
		return (0);
	}
	rd_consume(reader, (size_t)(nl - reader->data) + 1);
	reader->drop = 0;
	return (1);
}

/*
** Tamponda hazir bir satir varsa verir.
**
** Donen satir tamponun ICINE isaret eder ve BIR SONRAKI rd_take
** cagrisina kadar gecerlidir. Cagiran satiri saklayacaksa kopyalamali.
*/
t_readst	rd_take(t_reader *reader, char **line)
{
	char	*nl;
	size_t	len;

	*line = NULL;
	if (reader->pending > 0)
	{
		rd_consume(reader, reader->pending);
		reader->pending = 0;
	}
	if (reader->drop && rd_drain_drop(reader) == 0)
		return (RD_MORE);
	if (reader->len == 0)
		return (rd_no_newline(reader));
	nl = memchr(reader->data, '\n', reader->len);
	if (nl == NULL)
		return (rd_no_newline(reader));
	len = (size_t)(nl - reader->data);
	if (len > PROTO_MAX_LINE)
	{
		rd_consume(reader, len + 1);
		return (RD_TOO_LONG);
	}
	reader->data[len] = '\0';
	reader->pending = len + 1;
	*line = reader->data;
	return (RD_LINE);
}
