/*
** naxd.c - yardimci surecin yasam dongusu.
**
** HATA CIKISI ASLA TERMINALE GITMEZ:
**   Yardimci surecin stderr'i acilista bir gunluk dosyasina yonlendirilir.
**   Aksi halde karsi taraftaki bir hata izi kullanicinin ekranini
**   bozardi; bu sekilde bozmuyor ama kaybolmuyor da.
**
** YENIDEN DOGMA NEDEN UYUMADAN YAPILIYOR:
**   Bicim "0, 1 ve 4 saniye araliklarla denenir" diyor. Bunu uyuyarak
**   yapmak kabugun icinde saniyelerce beklemek olurdu - kullanici
**   komut yazamaz. Onun yerine BIR SONRAKI DENEMENIN EN ERKEN ZAMANI
**   kaydediliyor; o ana kadar gelen istek "su an yok" cevabi aliyor ve
**   kabuk akici kaliyor.
**
** YAZMA UCU BLOKE OLMAYAN KIPTE:
**   Yardimci surec okumayi birakirsa boru dolar. Bloke eden bir yazma o
**   noktada kabugu sonsuza kadar kilitlerdi ve okuma tarafindaki zaman
**   asimi hic devreye girmezdi - cunku surec write icinde asili kalir.
**   Bu yuzden yazma da sinirli beklenir ve sure dolarsa surec olu
**   sayilir. Mutasyon testi bu acigi gosterdi.
**
** SIGPIPE'I ISTEMCI KENDI YOK SAYAR:
**   Olmus surece yazmak varsayilan davranista yazan sureci oldurur.
**   Bu modulun dogrulugu buna bagli, o yuzden garanti uzaktaki bir
**   cagirana BIRAKILMIYOR. Ilk yazimda birakilmisti ve taklit surecle
**   kosan test SIGPIPE ile olmustu - kabukta degil testte, ama ayni
**   hata kabuga da gelebilirdi.
**
** UC BASARISIZLIK KURALI:
**   Bir dakika icinde uc kez basarisiz olursa AI kapatilir, oturumda BIR
**   KEZ uyari basilir ve kabuk duz kabuk olarak tam islevle devam eder.
**   Sayac penceresi gecince sifirlanir, yani gun icinde bir kez olen
**   surec AI'i kalici kapatmaz.
*/

#include "nax.h"
#include "naxd.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>

/* Tek yonlu bir sayac; milisaniye. */
long	naxd_now_ms(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return (0);
	return ((long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Baglantiyi kurar; surec henuz baslatilmaz. */
void	naxd_init(t_naxd *nx, char *const *argv, const char *log_path)
{
	nx->pid = -1;
	nx->in_fd = -1;
	rd_init(&nx->out, -1);
	nx->wake_rd = -1;
	nx->wake_wr = -1;
	nx->next_id = 0;
	nx->state = AI_OFF;
	nx->fails = 0;
	nx->tries = 0;
	nx->warned = 0;
	nx->first_fail_ms = 0;
	nx->next_try_ms = 0;
	nx->tick_ms = NAXD_TICK_MS;
	nx->limit_ms = NAXD_LIMIT_MS;
	nx->argv = argv;
	nx->log_path = log_path;
}

/* Surecin konusmaya hazir olup olmadigini soyler. */
int	naxd_alive(const t_naxd *nx)
{
	return (nx->state == AI_READY);
}

/* En son kullanilan istek kimligini verir. */
long	naxd_last_id(const t_naxd *nx)
{
	return (nx->next_id);
}

/* Sonucun okunabilir adini verir; testler ve gunluk icin. */
const char	*naxd_ask_name(t_askst state)
{
	static const char	*const	names[] = {
		"CONTINUE", "OK", "NEED", "ERR", "TIMEOUT", "CANCEL", "DEAD",
		"PROTOCOL", NULL
	};
	size_t				i;

	i = 0;
	while (names[i] != NULL)
	{
		if (i == (size_t)state)
			return (names[i]);
		i++;
	}
	return ("PROTOCOL");
}

/*
** Cocuk tarafini kurar ve calistirir; donmez.
**
** Gunluk acilamazsa stderr /dev/null'a gider: kullanicinin ekranini
** bozmamak, hata izini saklamaktan daha onemli.
*/
static void	run_child(t_naxd *nx, int to_child[2], int from_child[2])
{
	int	log_fd;

	close(to_child[1]);
	close(from_child[0]);
	dup2(to_child[0], STDIN_FILENO);
	dup2(from_child[1], STDOUT_FILENO);
	close(to_child[0]);
	close(from_child[1]);
	log_fd = open(nx->log_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
	if (log_fd < 0)
		log_fd = open("/dev/null", O_WRONLY);
	if (log_fd >= 0)
	{
		dup2(log_fd, STDERR_FILENO);
		close(log_fd);
	}
	if (nx->wake_rd >= 0)
		close(nx->wake_rd);
	if (nx->wake_wr >= 0)
		close(nx->wake_wr);
	sig_reset_child();
	execvp(nx->argv[0], nx->argv);
	_exit(127);
}

/* Kendine boruyu kurar ve yazma ucunu sinyal tarafina verir. */
static int	open_wake(t_naxd *nx)
{
	int	fds[2];

	if (pipe(fds) != 0)
		return (0);
	nx->wake_rd = fds[0];
	nx->wake_wr = fds[1];
	sig_set_wake_fd(nx->wake_wr);
	return (1);
}

/*
** Sureci catallar ve borulari baglar; basarida 1.
**
** Ana surec kullanmadigi uclari KAPATMAK zorunda: kapatilmazsa yardimci
** surec oldugunde okuma ucu dosya sonu gormez ve kabuk sonsuza kadar
** bekler.
*/
static int	spawn(t_naxd *nx)
{
	int	to_child[2];
	int	from_child[2];

	if (pipe(to_child) != 0)
		return (0);
	if (pipe(from_child) != 0)
	{
		close(to_child[0]);
		close(to_child[1]);
		return (0);
	}
	nx->pid = fork();
	if (nx->pid == 0)
		run_child(nx, to_child, from_child);
	close(to_child[0]);
	close(from_child[1]);
	if (nx->pid < 0)
	{
		close(to_child[1]);
		close(from_child[0]);
		return (0);
	}
	nx->in_fd = to_child[1];
	fcntl(nx->in_fd, F_SETFL, O_NONBLOCK);
	rd_init(&nx->out, from_child[0]);
	nx->state = AI_STARTING;
	return (1);
}

/* Surecin tanimlayicilarini kapatir ve cocugu toplar. */
static void	reap(t_naxd *nx)
{
	int	status;

	if (nx->in_fd >= 0)
		close(nx->in_fd);
	nx->in_fd = -1;
	if (nx->out.fd >= 0)
		close(nx->out.fd);
	rd_free(&nx->out);
	rd_init(&nx->out, -1);
	if (nx->pid > 0)
	{
		kill(nx->pid, SIGTERM);
		while (waitpid(nx->pid, &status, 0) < 0 && errno == EINTR)
			;
	}
	nx->pid = -1;
	nx->state = AI_OFF;
}

/*
** Basarisizligi kaydeder ve bir sonraki denemenin zamanini belirler.
**
** Araliklar 0, 1 ve 4 saniye - yani ILK yeniden deneme beklemez. Sira
** onemli: aralik sayac ARTIRILMADAN once okunuyor, yoksa ilk deneme bir
** saniye gecikirdi ve bicimdeki "0" hic kullanilmazdi.
**
** Pencere dolmussa sayac sifirlanir, yani birbirinden uzak iki olum
** AI'i kapatmaz.
*/
static void	note_failure(t_naxd *nx)
{
	static const long	backoff[3] = {0, 1000, 4000};
	long				now;

	now = naxd_now_ms();
	if (nx->fails == 0 || now - nx->first_fail_ms > NAXD_FAIL_WINDOW_MS)
	{
		nx->fails = 0;
		nx->first_fail_ms = now;
	}
	nx->fails++;
	nx->next_try_ms = now + backoff[nx->tries];
	if (nx->tries < 2)
		nx->tries++;
}

/*
** Sureci baslatir ve READY bekler; basarida 1.
**
** Basarisizlikta surec toplanir ve bir sonraki denemenin zamani
** kaydedilir. Uc basarisizliktan sonra artik denenmez.
*/
int	naxd_open(t_naxd *nx)
{
	t_frame	reply;
	t_askst	state;

	if (nx->fails >= NAXD_FAIL_MAX || naxd_now_ms() < nx->next_try_ms)
		return (0);
	sig_ignore_sigpipe();
	if (nx->wake_rd < 0 && open_wake(nx) == 0)
		return (0);
	if (spawn(nx) == 0)
	{
		note_failure(nx);
		return (0);
	}
	state = naxd_wait(nx, 0, &reply, NULL);
	if (state == ASK_OK && reply.type == FR_READY)
	{
		proto_free(&reply);
		nx->state = AI_READY;
		nx->tries = 0;
		return (1);
	}
	proto_free(&reply);
	reap(nx);
	note_failure(nx);
	return (0);
}

/* Veda eder, sureci kapatir ve kendine boruyu birakir. */
void	naxd_close(t_naxd *nx)
{
	if (nx->state != AI_OFF)
		naxd_send(nx, FR_BYE, NULL, 0);
	reap(nx);
	sig_set_wake_fd(-1);
	if (nx->wake_rd >= 0)
		close(nx->wake_rd);
	if (nx->wake_wr >= 0)
		close(nx->wake_wr);
	nx->wake_rd = -1;
	nx->wake_wr = -1;
}

/*
** Yazma ucunun bosalmasini sinirli sure bekler; yazilabilirse 1.
**
** NEDEN GEREKLI: yardimci surec okumayi birakirsa boru dolar ve write
** SONSUZA KADAR bloke olur. O anda zaman asimi islemiyor, cunku zaman
** asimi okuma tarafindaki poll'de. Kabugun hicbir kosulda
** kilitlenmemesi bu asamanin tek amaci, o yuzden yazma da sinirli
** beklenir.
**
** EINTR'de yazilabilir sayilir: karar bir sonraki yazma denemesine
** birakiliyor ve son tarih zaten mutlak, yani sinyal yagmuru sureyi
** uzatamaz.
*/
static int	wait_writable(t_naxd *nx, long deadline)
{
	struct pollfd	pfd;
	int				ready;
	long			left;

	left = deadline - naxd_now_ms();
	if (left <= 0)
		return (0);
	pfd.fd = nx->in_fd;
	pfd.events = POLLOUT;
	pfd.revents = 0;
	ready = poll(&pfd, 1, (int)left);
	if (ready < 0 && errno == EINTR)
		return (1);
	return (ready > 0);
}

/* Yazma basarisizligini kaydeder ve her zaman 0 doner. */
static int	write_failed(t_naxd *nx)
{
	nx->state = AI_OFF;
	return (0);
}

/*
** Satiri tamamen yazar; basarida 1.
**
** Kirik boruda SIGPIPE yok sayili oldugu icin EPIPE doner ve surec olu
** isaretlenir.
**
** KISMI YAZMA NORMALDIR, DONGU SART: yazma ucu bloke olmayan kipte
** oldugu icin buyuk bir kayit tek seferde gitmeyebilir. Dongusuz bir
** yazim kaydin yarisini gonderip basarili sayardi ve karsi taraf asla
** tam satir gormezdi.
**
** BASARISIZLIK TEK YERDE ISARETLENIR: iki cikis yolu da write_failed'e
** gidiyor. Ilk yazimda iki ayri atama vardi ve biri gereksizdi - cevap
** bekleyen yol zaten naxd_ask icinde isaretliyor. Mutasyon testi
** ikisinden birini silmenin hicbir vakayi bozmadigini gosterdi.
*/
int	naxd_write(t_naxd *nx, const char *line)
{
	size_t	len;
	size_t	done;
	ssize_t	wrote;
	long	deadline;

	if (nx->in_fd < 0)
		return (0);
	len = strlen(line);
	done = 0;
	deadline = naxd_now_ms() + nx->limit_ms;
	while (done < len)
	{
		wrote = write(nx->in_fd, line + done, len - done);
		if (wrote < 0 && errno == EINTR)
			continue ;
		if (wrote < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			if (wait_writable(nx, deadline))
				continue ;
			return (write_failed(nx));
		}
		if (wrote <= 0)
			return (write_failed(nx));
		done += (size_t)wrote;
	}
	return (1);
}

/* Cevap beklenmeyen bir kayit gonderir; basarida 1. */
int	naxd_send(t_naxd *nx, t_ftype type, const t_field *fields, size_t n)
{
	char	*line;
	int		ok;

	nx->next_id++;
	line = proto_build(type, nx->next_id, fields, n);
	if (line == NULL)
		return (0);
	ok = naxd_write(nx, line);
	free(line);
	return (ok);
}
