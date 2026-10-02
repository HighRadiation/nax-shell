/*
** naxd_wait.c - cevabi beklemek.
**
** AYNI ANDA IKI SEY GOZLENIR:
**   Yardimci surecin ciktisi ve kendine boru. Kullanici Ctrl-C'ye
**   bastiginda kesme isleyicisi o boruya bir bayt yaziyor ve poll
**   uyaniyor. Baska bir yolu yok: sinyal baglaminda guvenle yapilabilecek
**   is listesi cok kisa ve "bayrak set et" tek basina poll'u uyandirmaz.
**
** EINTR'DE POLL AYNI SURE ILE YENIDEN DENENMEZ:
**   Sinyal her geldiginde sayaci bastan baslatmak zaman asimini sonsuza
**   kadar oteleyebilirdi. Bunun yerine EINTR "henuz karar yok" sayilir ve
**   dis dongu kalan sureyi TEK YONLU SAYACTAN yeniden hesaplar. Boylece
**   on bes saniye, kac sinyal gelirse gelsin on bes saniye kaliyor.
**   Bu kosul docs/FINDINGS.md icinde acik madde olarak duruyordu.
**
** ESKI KIMLIKLER SESSIZCE ATILIR:
**   Zaman asimina ugramis bir istegin cevabi sonradan gelebilir. Kimlik
**   artan bir sayi oldugu icin beklenenden farkli olan her yanit atilir;
**   aksi halde kullanici bir onceki sorusunun cevabini gorurdu.
*/

#include "nax.h"
#include "naxd.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

/* Cercevedeki kimligi sayiya cevirir; alan yoksa -1. */
static long	frame_id(const t_frame *reply)
{
	const char	*text;

	text = proto_field(reply, "id");
	if (text == NULL)
		return (-1);
	return (atol(text));
}

/* Yanit tipini sonuca cevirir; beklenmeyen tip ihlal sayilir. */
static t_askst	classify(const t_frame *reply)
{
	if (reply->type == FR_OK || reply->type == FR_READY)
		return (ASK_OK);
	if (reply->type == FR_NEED)
		return (ASK_NEED);
	if (reply->type == FR_ERR)
		return (ASK_ERR);
	return (ASK_PROTOCOL);
}

/*
** Tamponda hazir satirlardan beklenen kimlige ait olani arar.
**
** ASK_CONTINUE donmesi "tamponda bu kimlik yok, akistan beslemek
** gerekiyor" demek.
*/
static t_askst	read_reply(t_naxd *nx, long id, t_frame *reply)
{
	t_readst	state;
	char		*line;
	t_perr		err;

	state = rd_take(&nx->out, &line);
	while (state == RD_LINE)
	{
		err = proto_parse(line, reply);
		if (err != PE_OK)
		{
			proto_free(reply);
			return (ASK_PROTOCOL);
		}
		if (frame_id(reply) == id)
			return (classify(reply));
		proto_free(reply);
		state = rd_take(&nx->out, &line);
	}
	if (state == RD_TOO_LONG)
		return (ASK_PROTOCOL);
	if (state == RD_EOF || state == RD_ERROR)
		return (ASK_DEAD);
	return (ASK_CONTINUE);
}

/* Kendine boruda bekleyen baytlari atar. */
static void	drain_wake(t_naxd *nx)
{
	char	buf[64];
	ssize_t	got;

	got = read(nx->wake_rd, buf, sizeof(buf));
	while (got < 0 && errno == EINTR)
		got = read(nx->wake_rd, buf, sizeof(buf));
	(void)got;
}

/*
** Verilen sure kadar iki tanimlayiciyi gozler.
**
** Kendine boru ONCE bakilir: kullanicinin iptali, ayni anda gelmis bir
** cevaptan onceliklidir. Aksi halde Ctrl-C'ye basildigi halde cevap
** islenebilirdi.
*/
static t_askst	wait_io(t_naxd *nx, int ms)
{
	struct pollfd	fds[2];
	int				ready;

	fds[0].fd = nx->out.fd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	fds[1].fd = nx->wake_rd;
	fds[1].events = POLLIN;
	fds[1].revents = 0;
	ready = poll(fds, 2, ms);
	if (ready < 0 && errno == EINTR)
		return (ASK_CONTINUE);
	if (ready < 0)
		return (ASK_DEAD);
	if (ready == 0)
		return (ASK_CONTINUE);
	if (fds[1].revents != 0)
	{
		drain_wake(nx);
		return (ASK_CANCEL);
	}
	if (rd_feed(&nx->out) == RD_ERROR)
		return (ASK_DEAD);
	return (ASK_CONTINUE);
}

/*
** Bir sonraki olaya kadarki sureyi verir.
**
** Gosterge esigi gecilmediyse oraya, gecildiyse sert sinira kadar
** beklenir. Boylece poll gereksiz yere erken uyanmiyor.
*/
static int	slice_ms(const t_naxd *nx, long spent, int ticked)
{
	if (ticked == 0 && spent < nx->tick_ms)
		return ((int)(nx->tick_ms - spent));
	return ((int)(nx->limit_ms - spent));
}

/*
** Beklenen kimlige ait cevap gelene kadar bekler.
**
** Cerceve yalnizca ASK_OK, ASK_NEED ve ASK_ERR durumlarinda doludur;
** cagiran her durumda proto_free cagirabilir.
**
** CERCEVE BASTA KURULUR, SERBEST BIRAKILMAZ: bu islev kurulmamis bir
** cerceve ile cagrilabiliyor ve cop isaretciyi serbest birakmak
** cokertirdi. Eski icerigi birakmak cagiranin isi.
*/
t_askst	naxd_wait(t_naxd *nx, long id, t_frame *reply, t_tick_fn tick)
{
	long	start;
	long	spent;
	t_askst	state;
	int		ticked;

	proto_blank(reply);
	start = naxd_now_ms();
	ticked = 0;
	while (1)
	{
		state = read_reply(nx, id, reply);
		if (state != ASK_CONTINUE)
			return (state);
		spent = naxd_now_ms() - start;
		if (spent >= nx->limit_ms)
			return (ASK_TIMEOUT);
		if (ticked == 0 && spent >= nx->tick_ms)
		{
			ticked = 1;
			if (tick != NULL)
				tick();
		}
		state = wait_io(nx, slice_ms(nx, spent, ticked));
		if (state != ASK_CONTINUE)
			return (state);
	}
}

/* Zaman asimi ya da iptalde karsi tarafa vazgectigimizi bildirir. */
static void	send_cancel(t_naxd *nx, long id)
{
	char	*line;

	line = proto_build(FR_CANCEL, id, NULL, 0);
	if (line == NULL)
		return ;
	naxd_write(nx, line);
	free(line);
}

/*
** Bir istek gonderir ve cevabini bekler.
**
** Zaman asimi ve iptalde CANCEL gonderilir: karsi taraf bos yere
** calismaya devam etmesin ve gec gelen cevabi uretmesin diye.
**
** OLUM VE IHLAL BAGLANTIYI KAPATIR. Ilk yazimda yalnizca YAZMA yolu
** durumu guncelliyordu; okuma tarafinda dosya sonu gorulerek anlasilan
** olum durumu degistirmiyordu ve naxd_alive olmus bir surece "hazir"
** diyordu. Ihlal de kapatiyor, cunku bicim bozuk konusan sureci yeniden
** baslatmayi gerektiriyor.
**
** ZAMAN ASIMI KAPATMAZ: gec cevap veren surec bozuk degil, yavas. CANCEL
** gonderilip devam ediliyor.
*/
t_askst	naxd_ask(t_naxd *nx, t_ftype type, const t_pair *fields, size_t n,
		t_frame *reply, t_tick_fn tick)
{
	char	*line;
	long	id;
	t_askst	state;

	proto_blank(reply);
	if (nx->in_fd < 0)
		return (ASK_DEAD);
	nx->next_id++;
	id = nx->next_id;
	line = proto_build(type, id, fields, n);
	if (line == NULL)
		return (ASK_PROTOCOL);
	state = ASK_DEAD;
	if (naxd_write(nx, line))
		state = naxd_wait(nx, id, reply, tick);
	free(line);
	if (state == ASK_TIMEOUT || state == ASK_CANCEL)
		send_cancel(nx, id);
	if (state == ASK_DEAD || state == ASK_PROTOCOL)
		nx->state = AI_OFF;
	return (state);
}
