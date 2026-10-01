/*
** signal.c - etkilesimli kabugun sinyal davranisi.
**
** NEDEN AYRI MODUL:
**   Sinyal yonetimi kabugun en ince kismidir ve tek bir yerde toplanmasi
**   gerekir. Buradaki her karar olculerek verildi; gerekceleri asagida.
**
** OLCULEN TUZAK 1 - readline kabugu olduruyor:
**   rl_catch_signals varsayilan olarak 1'dir. readline SIGINT'i kendi
**   yakalar, ekrani temizler ve sinyali yeniden gonderir; varsayilan eylem
**   sureci oldurur. Yani hicbir sey yapmazsan Ctrl-C kabugu kapatir.
**
** OLCULEN TUZAK 2 - bayrak set etmek tek basina YETMEZ:
**   rl_catch_signals'i kapatip isleyicide yalnizca bir bayrak set etmek
**   kabugu olmekten kurtarir ama satiri atmaz: readline EINTR'de NULL
**   donmez, okumayi iceride yeniden dener ve yarim satiri tamponda tutar.
**   Olculdu: "yarim satir" + Ctrl-C + "sonrasi" tek satir olarak birlesti.
**
** COZUM - rl_event_hook:
**   readline girdi beklerken bu kancayi periyodik olarak cagirir ve bu
**   cagri NORMAL baglamda olur, sinyal baglaminda degil. Tamponu
**   temizlemek ve rl_done set etmek orada guvenlidir. Sinyal isleyicisi
**   yalnizca bir bayrak set eder; sinyal baglaminda guvenli olmayan tek
**   bir cagri bile yapilmaz.
**
** SIGPIPE NEDEN YOK SAYILIYOR:
**   Yardimci surec oldugunde kabuk onun borusuna yazmaya calisir.
**   Varsayilan davranis kabugu OLDURMEK olurdu; yok sayildiginda yazma
**   EPIPE hatasi dondurur ve kabuk bunu normal bir hata gibi ele alir.
**   Yani AI'in olumu kabugu goturmez.
**
**   Cocuk tarafi sig_reset_child icinde temizliyor ve orada elle tutulan
**   bir liste YOK; gerekcesi o islevin basinda.
**
**   TESTI SAHTE TERMINALDE KOSMAK ZORUNDA: bu islev yalnizca etkilesimli
**   kipte cagriliyor, yani boruyla beslenen bir kabukta devralinacak bir
**   sey olmuyor. Ilk yazimda vaka boru grubundaydi ve sifirlama kasten
**   silindiginde patlamadi.
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

#define SIG_HIGHEST 31

static volatile sig_atomic_t	g_interrupted;
static char						g_inherited[SIG_HIGHEST + 1];

/*
** Kabuga GIRILIRKEN yok sayilan sinyalleri kaydeder.
**
** Acilista, readline kurulmadan once cagrilmak zorunda: readline kendi
** adina sinyal yok sayiyor ve o dagilim "devralinan" sayilmamali.
*/
void	sig_snapshot_inherited(void)
{
	struct sigaction	old;
	int					sig;

	sig = 1;
	while (sig <= SIG_HIGHEST)
	{
		if (sig != SIGKILL && sig != SIGSTOP
			&& sigaction(sig, NULL, &old) == 0)
			g_inherited[sig] = (old.sa_handler == SIG_IGN);
		sig++;
	}
}

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
	signal(SIGPIPE, SIG_IGN);
	rl_catch_signals = 0;
	rl_event_hook = on_readline_wait;
}

/*
** Cocuk surecte sinyal dagilimini duzeltir.
**
** NEDEN ZORUNLU:
**   execve YAKALANAN sinyalleri varsayilana dondurur ama YOK SAYILAN
**   sinyalleri yok sayili BIRAKIR. Sifirlanmazsa her cocuk kabugun yok
**   saydiklarini devralir; SIGPIPE ornegi ozellikle sinsi, cunku
**   "yes | head" gibi bir hatta yazan taraf olmez.
**
** NEDEN ELLE TUTULAN LISTE DEGIL:
**   Ilk yazim yalnizca kabugun kendi yok saydiklarini sifirliyordu ve
**   "yeni bir sinyal yok saymaya baslarsan buraya ekle" diye bir bakim
**   kurali vardi. O kural YETERSIZ: olculdu, readline kabuk adina
**   SIGPIPE ve SIGXFSZ'yi yok sayiyor. Yani cocuklar bizim hic
**   yazmadigimiz bir dagilimi devraliyordu. Artik tum sinyaller
**   geziliyor ve hatirlanacak liste yok.
**
** DEVRALINAN YOK SAYMA KORUNUR:
**   Kabuk SIGPIPE yok sayili halde baslatildiysa cocuklar da onu yok
**   sayili gorur. Bash boyle davraniyor (olculdu: yok sayili girildiginde
**   cocuk maskesi 0x1000, varsayilan girildiginde 0). Bu yuzden acilista
**   bir anlik goruntu alinir ve burada yalnizca SONRADAN eklenenler
**   temizlenir.
*/
void	sig_reset_child(void)
{
	int	sig;

	sig = 1;
	while (sig <= SIG_HIGHEST)
	{
		if (sig != SIGKILL && sig != SIGSTOP)
		{
			if (g_inherited[sig])
				signal(sig, SIG_IGN);
			else
				signal(sig, SIG_DFL);
		}
		sig++;
	}
}

/* Kesme olup olmadigini soyler ve bayragi sifirlar. */
int	sig_take_interrupt(void)
{
	int	seen;

	seen = g_interrupted;
	g_interrupted = 0;
	return (seen);
}
