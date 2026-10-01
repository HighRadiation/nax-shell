/*
** test_lineio.c - akistan tam kayit satirlari toplamak.
**
** NEDEN SENARYO TABLOSU:
**   Okuyucunun asil isi parcali veriyi dogru birlestirmek. Tek bir girdi
**   ile olculemez; verinin HANGI parcalar halinde geldigi onemli. Bu
**   yuzden her vaka bir senaryo: boruya sirayla yazilan parcalar ve
**   bunlarin urettigi olay dizisi.
**
** GIRDI BICIMI:
**   Parcalar "|" ile ayrilir. Parca icinde "<NL>" bir yenisatir baytidir.
**   Tek basina "~" olan parca YAZMA UCUNU KAPATIR, yani akisi bitirir.
**
** BEKLENEN BICIMI:
**   Olaylar ";" ile ayrilir: "LINE:<metin>", "TOOLONG", "EOF", "ERROR".
**
** HER PARCADA BIR OKUMA:
**   Yazma ucu acikken ikinci okuma bloke olurdu, cunku boruda veri
**   kalmaz. Bu yuzden parca basina bir okuma yapilir; yazma ucu
**   kapandiktan sonra okuma bloke olmayacagi icin akis bitene kadar
**   okunur.
**
** SINIR VAKALARI TABLODA DEGIL:
**   Asiri uzun satiri olcmek icin bir mebibayttan fazla yazmak gerekiyor,
**   oysa boru tamponu bundan cok kucuk ve yazma bloke olurdu. O vakalar
**   gecici dosya uzerinden, C icindeki adli kontrollerde.
*/

#include "nax.h"
#include "proto.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* Parcadaki <NL> isaretlerini gercek yenisatira cevirip boruya yazar. */
static void	write_chunk(int fd, const char *chunk)
{
	const char	*mark;

	while (*chunk != '\0')
	{
		mark = strstr(chunk, "<NL>");
		if (mark == NULL)
		{
			write(fd, chunk, strlen(chunk));
			return ;
		}
		write(fd, chunk, (size_t)(mark - chunk));
		write(fd, "\n", 1);
		chunk = mark + 4;
	}
}

/* Bir olayi sonuc dizgisine ekler. */
static void	record(char *out, size_t cap, const char *name, const char *text)
{
	if (out[0] != '\0')
		test_append(out, cap, ";");
	test_append(out, cap, name);
	if (text != NULL)
	{
		test_append(out, cap, ":");
		test_append(out, cap, text);
	}
}

/* Tamponu bosaltir; izin verilen okuma sayisi kadar akistan besler. */
static void	drain(t_reader *reader, char *out, size_t cap, int feeds)
{
	t_readst	state;
	char		*line;

	while (1)
	{
		state = rd_take(reader, &line);
		if (state == RD_LINE)
			record(out, cap, "LINE", line);
		else if (state == RD_TOO_LONG)
			record(out, cap, "TOOLONG", NULL);
		else if (state == RD_EOF)
		{
			record(out, cap, "EOF", NULL);
			return ;
		}
		else if (feeds <= 0)
			return ;
		else
		{
			feeds--;
			state = rd_feed(reader);
			if (state == RD_ERROR)
			{
				record(out, cap, "ERROR", NULL);
				return ;
			}
		}
	}
}

/* Senaryoyu kosar: parcalari sirayla yazip olaylari toplar. */
static void	play(char *input, char *out, size_t cap)
{
	t_reader	reader;
	int			fds[2];
	char		*chunk;
	int			closed;

	if (pipe(fds) != 0)
	{
		test_append(out, cap, "BORU-KURULAMADI");
		return ;
	}
	rd_init(&reader, fds[0]);
	closed = 0;
	chunk = strtok(input, "|");
	while (chunk != NULL)
	{
		if (strcmp(chunk, "~") == 0 && closed == 0)
		{
			close(fds[1]);
			closed = 1;
		}
		else
			write_chunk(fds[1], chunk);
		drain(&reader, out, cap, 1 + closed * 64);
		chunk = strtok(NULL, "|");
	}
	rd_free(&reader);
	close(fds[0]);
	if (closed == 0)
		close(fds[1]);
}

/* Senaryo vakasi. */
static void	case_play(t_score *score, int no, char *input, char *want)
{
	char	got[4096];

	got[0] = '\0';
	play(input, got, sizeof(got));
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
}

/* Verilen icerigi gecici dosyaya yazip okuma tanimlayicisini dondurur. */
static int	temp_fd(const char *data, size_t n)
{
	char	path[64];
	int		fd;

	snprintf(path, sizeof(path), "/tmp/nax_lineio_XXXXXX");
	fd = mkstemp(path);
	if (fd < 0)
		return (-1);
	unlink(path);
	if (write(fd, data, n) < 0 || lseek(fd, 0, SEEK_SET) != 0)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

/* Dosyadan okuyup olaylari toplar; dosya oldugu icin okuma bloke olmaz. */
static void	play_fd(int fd, char *out, size_t cap)
{
	t_reader	reader;

	rd_init(&reader, fd);
	drain(&reader, out, cap, 4096);
	rd_free(&reader);
	close(fd);
}

/*
** Sinirin ustundeki satir TOOLONG verir ve kalani atlanir.
**
** Icerik: bir mebibayt arti bir "x", yenisatir, sonra "SONRA" satiri.
** Beklenen: asiri uzun satir atilir, ARDINDAN gelen satir normal okunur.
** Kurtarma yetenegi olculen sey; yoksa tek bozuk kayit akisi zehirler.
*/
static int	check_too_long_then_recover(void)
{
	char	*data;
	char	got[256];
	size_t	n;
	int		fd;

	n = PROTO_MAX_LINE + 1;
	data = malloc(n + 8);
	if (data == NULL)
		return (0);
	memset(data, 'x', n);
	memcpy(data + n, "\nSONRA\n", 7);
	fd = temp_fd(data, n + 7);
	free(data);
	if (fd < 0)
		return (0);
	got[0] = '\0';
	play_fd(fd, got, sizeof(got));
	return (strcmp(got, "TOOLONG;LINE:SONRA;EOF") == 0);
}

/* Dosyadan ilk satiri okur; durumu dondurur ve uzunlugu yazar. */
static t_readst	first_line_len(int fd, size_t *len)
{
	t_reader	reader;
	char		*line;
	t_readst	state;

	rd_init(&reader, fd);
	state = rd_take(&reader, &line);
	while (state == RD_MORE)
	{
		if (rd_feed(&reader) == RD_ERROR)
			break ;
		state = rd_take(&reader, &line);
	}
	*len = 0;
	if (state == RD_LINE)
		*len = strlen(line);
	rd_free(&reader);
	close(fd);
	return (state);
}

/*
** Siniri TAM tutan satir kabul edilir.
**
** Olay dizgisi uzerinden olculmuyor: satir bir mebibayt ve olaylari
** toplayan tampon o kadar buyuk degil, yani dizgi kesilirdi. Ilk yazim
** boyle olculuyordu ve vaka kodda bir sorun olmadigi halde patliyordu.
** Dogrudan satirin uzunluguna bakmak hem dogru hem ucuz.
*/
static int	check_exact_limit(void)
{
	char	*data;
	size_t	n;
	size_t	got;
	int		fd;

	n = PROTO_MAX_LINE;
	data = malloc(n + 2);
	if (data == NULL)
		return (0);
	memset(data, 'y', n);
	data[n] = '\n';
	fd = temp_fd(data, n + 1);
	free(data);
	if (fd < 0)
		return (0);
	return (first_line_len(fd, &got) == RD_LINE && got == n);
}

/* Adi verilen kontrolu kosar. */
static void	case_check(t_score *score, int no, char *input, char *want)
{
	int	ok;

	if (strcmp(input, "too_long_then_recover") == 0)
		ok = check_too_long_then_recover();
	else if (strcmp(input, "exact_limit") == 0)
		ok = check_exact_limit();
	else
	{
		report_fail(score, no, input, want, "boyle bir kontrol yok");
		return ;
	}
	if (ok)
		score->passed++;
	else
		report_fail(score, no, input, want, "kontrol basarisiz");
}

/* Vakayi onekine gore yonlendirir. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	if (strncmp(input, "C:", 2) == 0)
		case_check(score, no, input + 2, want);
	else
		case_play(score, no, input, want);
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*given;

	given = "test/cases/lineio.tsv";
	if (argc > 1)
		given = argv[1];
	return (harness_run(given, "lineio testleri", run_case));
}
