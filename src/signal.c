/*
** signal.c — etkilesimli kabugun sinyal davranisi.
**
** NEDEN AYRI MODUL:
**   Sinyal yonetimi kabugun en ince kismidir ve tek bir yerde toplanmasi
**   gerekir. Buradaki her karar olculerek verildi; gerekceleri asagida.
**
** OLCULEN TUZAK 1 — readline kabugu olduruyor:
**   rl_catch_signals varsayilan olarak 1'dir. readline SIGINT'i kendi
**   yakalar, ekrani temizler ve sinyali yeniden gonderir; varsayilan eylem
**   sureci oldurur. Yani hicbir sey yapmazsan Ctrl-C kabugu kapatir.
**
** OLCULEN TUZAK 2 — bayrak set etmek tek basina YETMEZ:
**   rl_catch_signals'i kapatip isleyicide yalnizca bir bayrak set etmek
**   kabugu olmekten kurtarir ama satiri atmaz: readline EINTR'de NULL
**   donmez, okumayi iceride yeniden dener ve yarim satiri tamponda tutar.
**   Olculdu: "yarim satir" + Ctrl-C + "sonrasi" tek satir olarak birlesti.
**
** COZUM — rl_event_hook:
**   readline girdi beklerken bu kancayi periyodik olarak cagirir ve bu
**   cagri NORMAL baglamda olur, sinyal baglaminda degil. Tamponu
**   temizlemek ve rl_done set etmek orada guvenlidir. Sinyal isleyicisi
**   yalnizca bir bayrak set eder; sinyal baglaminda guvenli olmayan tek
**   bir cagri bile yapilmaz.
**
** SA_RESTART BILINCLI OLARAK YOK:
**   Bloke eden cagrilarin EINTR donmesi isteniyor. Bunun bedeli var:
**   calistirici geldiginde waitpid, yardimci surec beklemesi geldiginde
**   poll EINTR dongusune sarilmak ZORUNDA. Sarilmazsa Ctrl-C bir komut
**   beklenirken bozuk davranir. Bu kosul docs/FINDINGS.md'de kayitlidir.
*/

#include "nax.h"
#include <signal.h>
#include <unistd.h>
#include <readline/readline.h>

static volatile sig_atomic_t	g_interrupted;

/* SIGINT isleyicisi; sinyal baglaminda guvenli olan tek isi yapar. */
static void	on_sigint(int sig)
{
	(void)sig;
	g_interrupted = 1;
}

/* readline girdi beklerken normal baglamda cagrilir; satiri iptal eder. */
static int	on_readline_wait(void)
{
	if (g_interrupted == 0)
		return (0);
	write(STDOUT_FILENO, "^C", 2);
	rl_replace_line("", 0);
	rl_done = 1;
	return (0);
}

/* Etkilesimli kabugun sinyal davranisini kurar. */
void	sig_setup_interactive(void)
{
	struct sigaction	sa;

	sa.sa_handler = on_sigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	signal(SIGQUIT, SIG_IGN);
	rl_catch_signals = 0;
	rl_event_hook = on_readline_wait;
}

/*
** Cocuk surecte sinyalleri varsayilana dondurur.
**
** NEDEN ZORUNLU:
**   execve YAKALANAN sinyalleri varsayilana dondurur ama YOK SAYILAN
**   sinyalleri yok sayili BIRAKIR. Kabuk SIGQUIT'i yok sayiyor;
**   sifirlanmazsa her cocuk bunu devralir ve Ctrl-\ hicbir komutu
**   etkilemez. SIGINT'in isleyicisi execve tarafindan zaten sifirlaniyor,
**   ama niyeti gorunur kilmak icin burada da yazili.
**
** BAKIM KURALI:
**   Kabuk ileride baska bir sinyali yok saymaya baslarsa (ornegin boru
**   hatlari icin SIGPIPE), o sinyal BURAYA da eklenmek zorunda. Yoksa
**   cocuklar sessizce devralir; SIGPIPE ornegi ozellikle sinsi, cunku
**   "yes | head" gibi bir hat yazan taraf olmedigi icin asili kalir.
*/
void	sig_reset_child(void)
{
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
}

/* Kesme olup olmadigini soyler ve bayragi sifirlar. */
int	sig_take_interrupt(void)
{
	int	seen;

	seen = g_interrupted;
	g_interrupted = 0;
	return (seen);
}
