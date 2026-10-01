/*
** test_naxd.c - yardimci surecle konusmanin dayanikliligi.
**
** NEDEN TAKLIT SUREC:
**   Bu asamanin sorusu "cevap dogru mu" degil, "karsi taraf olur, donar
**   ya da sacmalarsa kabuk saglam kaliyor mu". Gercek bir modelle bu
**   durumlari uretmek hem yavas hem rastgele olurdu; taklit her birini
**   istege gore uretiyor. Kipler test/fake_naxd.py basliginda.
**
** GIRDI BICIMI:
**   <kip>[:gecikme]   Taklit bu kiple baslatilir.
**
** BEKLENEN BICIMI:
**   ";" ile ayrilmis olaylar:
**     OPEN:ok | OPEN:fail       Acilis sonucu
**     TICK                      Bekleme uzadi gostergesi tetiklendi
**     ASK:<sonuc>               Istegin sonucu (OK, DEAD, TIMEOUT...)
**     CMD:<metin>               Gelen onerinin icerigi
**     STATE:ready | STATE:off   Istek sonrasi baglantinin durumu
**     ALIVE                     Her seyden sonra kabuk tarafi saglam
**
** ZAMAN ASIMLARI KISALTILIYOR:
**   Gercek degerler dort ve on bes saniye. Testin bunlari beklemesi
**   kabul edilemez, o yuzden yapidaki alanlar milisaniyeye cekiliyor.
**   Sabit degil alan olmalarinin sebebi de bu; ayni degerler ileride
**   yapilandirmadan gelecek.
*/

#include "nax.h"
#include "naxd.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#define TEST_TICK_MS 150
#define TEST_LIMIT_MS 1200

static int	g_ticks;

/* Gosterge kancasi; kac kez cagrildigini sayar. */
static void	on_tick(void)
{
	g_ticks++;
}

/* Gunluk dosyasinin yolunu verir. */
static const char	*log_path(void)
{
	return ("/tmp/nax_fake_naxd.log");
}

/* Taklidi verilen kiple baslatacak arguman dizisini kurar. */
static void	build_argv(char *argv[5], char *mode, char *delay)
{
	argv[0] = (char *)"python3";
	argv[1] = (char *)"test/fake_naxd.py";
	argv[2] = mode;
	argv[3] = delay;
	argv[4] = NULL;
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

/* Istegi gonderip sonucunu ve varsa onerisini kaydeder. */
static void	do_ask(t_naxd *nx, char *out, size_t cap)
{
	t_field		field;
	t_frame		reply;
	t_askst		state;
	const char	*cmd;

	field.key = "text";
	field.value = "dun degisen dosyalar";
	proto_blank(&reply);
	state = naxd_ask(nx, FR_INTENT, &field, 1, &reply, on_tick);
	if (g_ticks > 0)
		record(out, cap, "TICK", NULL);
	record(out, cap, "ASK", naxd_ask_name(state));
	if (state == ASK_OK)
	{
		cmd = proto_field(&reply, "cmd");
		if (cmd == NULL)
			cmd = proto_field(&reply, "text");
		record(out, cap, "CMD", cmd);
	}
	if (naxd_alive(nx))
		record(out, cap, "STATE", "ready");
	else
		record(out, cap, "STATE", "off");
	proto_free(&reply);
}

/* Senaryoyu kosar: taklidi baslatir, bir istek yapar, kapatir. */
static void	play(char *input, char *out, size_t cap)
{
	t_naxd	nx;
	char	*argv[5];
	char	*delay;

	delay = strchr(input, ':');
	if (delay != NULL)
		*delay++ = '\0';
	else
		delay = (char *)"0.3";
	build_argv(argv, input, delay);
	g_ticks = 0;
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS;
	if (naxd_open(&nx) == 0)
		record(out, cap, "OPEN", "fail");
	else
	{
		record(out, cap, "OPEN", "ok");
		do_ask(&nx, out, cap);
	}
	naxd_close(&nx);
	record(out, cap, "ALIVE", NULL);
}

/* Senaryo vakasi. */
static void	case_play(t_score *score, int no, char *input, char *want)
{
	char	got[1024];
	char	copy[256];

	snprintf(copy, sizeof(copy), "%s", input);
	got[0] = '\0';
	play(copy, got, sizeof(got));
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
}

/*
** Taklidin hata cikisi terminale SIZMAMALI.
**
** Gurultulu kipte iki yuz satir yazdiriliyor. Gunluk dosyasinda
** gorulmeli, ama bu sureci besleyen hicbir yere yazilmamali - aksi halde
** karsi taraftaki bir hata izi kullanicinin ekranini bozardi.
*/
static int	check_stderr_goes_to_log(void)
{
	t_naxd	nx;
	char	*argv[5];
	char	line[256];
	FILE	*fp;
	int		found;

	unlink(log_path());
	build_argv(argv, (char *)"noisy_stderr", (char *)"0.1");
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS;
	if (naxd_open(&nx) == 0)
		return (0);
	naxd_close(&nx);
	fp = fopen(log_path(), "r");
	if (fp == NULL)
		return (0);
	found = 0;
	while (fgets(line, sizeof(line), fp) != NULL)
		if (strstr(line, "taklit gurultu satiri") != NULL)
			found++;
	fclose(fp);
	return (found >= 200);
}

/*
** Ust uste basarisizliktan sonra artik denenmez.
**
** Kural: bir dakika icinde uc basarisizlik AI'i kapatir. Dorduncu
** deneme hic surec catallamamali, cunku surekli olen bir yardimciyi
** sonsuza kadar yeniden baslatmak kabugu her istekte yavaslatirdi.
*/
static int	check_gives_up_after_three(void)
{
	t_naxd	nx;
	char	*argv[5];
	int		i;
	int		opened;

	build_argv(argv, (char *)"die_at_start", (char *)"0.1");
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS;
	opened = 0;
	i = 0;
	while (i < 3)
	{
		nx.next_try_ms = 0;
		if (naxd_open(&nx))
			opened++;
		i++;
	}
	nx.next_try_ms = 0;
	if (naxd_open(&nx))
		opened++;
	naxd_close(&nx);
	return (opened == 0 && nx.fails == NAXD_FAIL_MAX);
}

/*
** Kendine boruya gelen bayt beklemeyi keser.
**
** Gercek hayatta o bayti kesme isleyicisi yaziyor. Test sinyal
** gondermek yerine dogrudan boruya yaziyor: olculmek istenen sey
** sinyalin kendisi degil, BEKLEMENIN o bayti gorup birakmasi. Sinyal
** yolunun tamami kabuga baglandiginda sahte terminalde olculecek.
**
** Kip "silent", yani cevap asla gelmeyecek; iptal olmasa zaman asimina
** ugrardi. Sonucun CANCEL olmasi, iptalin zaman asimindan ONCE
** goruldugunu kanitliyor.
*/
static int	check_cancel_wakes_wait(void)
{
	t_naxd	nx;
	char	*argv[5];
	t_frame	reply;
	t_field	field;
	t_askst	state;
	char	byte;

	build_argv(argv, (char *)"silent", (char *)"0.1");
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS;
	if (naxd_open(&nx) == 0)
		return (0);
	byte = 1;
	if (write(nx.wake_wr, &byte, 1) != 1)
		return (0);
	field.key = "text";
	field.value = "bir sey";
	proto_blank(&reply);
	state = naxd_ask(&nx, FR_INTENT, &field, 1, &reply, on_tick);
	proto_free(&reply);
	naxd_close(&nx);
	return (state == ASK_CANCEL);
}

/*
** Yeniden deneme araliklari: ILK deneme beklemez, IKINCISI bekler.
**
** Bicim "0, 1 ve 4 saniye" diyor. Ilk yazimda sayac araliktan ONCE
** artiriliyordu, yani ilk deneme bir saniye gecikiyor ve bicimdeki "0"
** hic kullanilmiyordu.
*/
static int	check_backoff_delays_retry(void)
{
	t_naxd	nx;
	char	*argv[5];
	int		first;
	int		second;

	build_argv(argv, (char *)"die_at_start", (char *)"0.1");
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS;
	naxd_open(&nx);
	first = nx.fails;
	naxd_open(&nx);
	second = nx.fails;
	naxd_open(&nx);
	naxd_close(&nx);
	return (first == 1 && second == 2 && nx.fails == 2);
}

/*
** Sagir surece buyuk kayit yazmak kilitlenmemeli.
**
** Yardimci surec stdin'i hic okumadiginda boru doluyor. Yazma sinirli
** beklenmezse surec write icinde asili kalir ve okuma tarafindaki zaman
** asimi hic devreye girmez - yani kabuk kilitlenir. Dogru davranis:
** sure dolunca surec olu sayilir ve kabuk devam eder.
**
** Kayit boru tamponundan BUYUK olmak zorunda, yoksa yazma tek seferde
** biter ve tikanma hic yasanmaz.
*/
static int	check_deaf_write_does_not_hang(void)
{
	t_naxd	nx;
	char	*argv[5];
	t_frame	reply;
	t_field	field;
	t_askst	state;
	char	*big;

	build_argv(argv, (char *)"deaf", (char *)"0.1");
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS;
	if (naxd_open(&nx) == 0)
		return (0);
	big = malloc(300000);
	if (big == NULL)
		return (0);
	memset(big, 'k', 299999);
	big[299999] = '\0';
	field.key = "text";
	field.value = big;
	proto_blank(&reply);
	state = naxd_ask(&nx, FR_INTENT, &field, 1, &reply, on_tick);
	proto_free(&reply);
	free(big);
	naxd_close(&nx);
	return (state == ASK_DEAD && nx.state == AI_OFF);
}

/*
** Cevap beklenmeyen gonderim de olumu gormeli.
**
** EVENT ve BYE gibi kayitlar cevap beklemiyor, yani naxd_ask'in olum
** isaretlemesi onlari KAPSAMIYOR. Yazma yolunun kendi isaretlemesi bu
** durumda tek koruma; olmasa kabuk olmus bir surece olay gondermeye
** devam ederdi.
**
** Mutasyon testi bu boslugu gosterdi: yazma yolundaki isaretleme
** silindiginde hicbir vaka patlamiyordu.
**
** DURUM KAPANISTAN ONCE OKUNUYOR: naxd_close zaten durumu sifirliyor,
** yani sonra bakmak her zaman "kapali" gorurdu ve vaka yine hicbir sey
** olcmezdi. Ilk yazim boyleydi.
*/
static int	check_event_write_marks_dead(void)
{
	t_naxd	nx;
	char	*argv[5];
	t_field	field;
	int		sent;
	int		marked;

	build_argv(argv, (char *)"die_after_ready", (char *)"0.1");
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS;
	if (naxd_open(&nx) == 0)
		return (0);
	usleep(200000);
	field.key = "cmd";
	field.value = "ls";
	sent = naxd_send(&nx, FR_EVENT, &field, 1);
	if (sent)
		sent = naxd_send(&nx, FR_EVENT, &field, 1);
	marked = (naxd_alive(&nx) == 0);
	naxd_close(&nx);
	return (sent == 0 && marked);
}

/*
** Gecici tikanma baglantiyi OLDURMEMELI.
**
** Yardimci surec bir sure okumayip sonra okumaya baslarsa boru gecici
** olarak doluyor. Dogru davranis yazilabilir olmasini beklemek; hemen
** vazgecmek, buyuk bir baglam gonderildiginde baglantiyi her seferinde
** koparirdi.
**
** Sagir surec vakasiyla ayni kayit boyutu kullaniliyor, tek fark karsi
** tarafin sonunda OKUMASI.
*/
static int	check_late_reader_recovers(void)
{
	t_naxd	nx;
	char	*argv[5];
	t_frame	reply;
	t_field	field;
	t_askst	state;
	char	*big;

	build_argv(argv, (char *)"late_reader", (char *)"0.3");
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS * 3;
	if (naxd_open(&nx) == 0)
		return (0);
	big = malloc(300000);
	if (big == NULL)
		return (0);
	memset(big, 'k', 299999);
	big[299999] = '\0';
	field.key = "text";
	field.value = big;
	proto_blank(&reply);
	state = naxd_ask(&nx, FR_INTENT, &field, 1, &reply, on_tick);
	proto_free(&reply);
	free(big);
	naxd_close(&nx);
	return (state == ASK_OK);
}

/*
** Poll sinyalle kesilse de zaman asimi uzamamali.
**
** Yuz elli milisaniyede bir SIGALRM gonderiliyor, yani poll on kadar kez
** EINTR ile donuyor. Iki sey birlikte olculuyor:
**   - EINTR olum sayilmiyor (sonuc DEAD degil TIMEOUT)
**   - sayac her kesilmede bastan BASLAMIYOR; aksi halde toplam sure
**     sinirin katlarina cikardi
*/
static void	on_alarm(int sig)
{
	(void)sig;
}

static int	check_eintr_does_not_extend(void)
{
	t_naxd				nx;
	char				*argv[5];
	t_frame				reply;
	t_field				field;
	t_askst				state;
	struct itimerval	timer;
	struct sigaction	sa;
	long				spent;

	build_argv(argv, (char *)"silent", (char *)"0.1");
	naxd_init(&nx, argv, log_path());
	nx.tick_ms = TEST_TICK_MS;
	nx.limit_ms = TEST_LIMIT_MS;
	if (naxd_open(&nx) == 0)
		return (0);
	sa.sa_handler = on_alarm;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, NULL);
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 150000;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 150000;
	setitimer(ITIMER_REAL, &timer, NULL);
	field.key = "text";
	field.value = "bir sey";
	proto_blank(&reply);
	spent = naxd_now_ms();
	state = naxd_ask(&nx, FR_INTENT, &field, 1, &reply, on_tick);
	spent = naxd_now_ms() - spent;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);
	proto_free(&reply);
	naxd_close(&nx);
	return (state == ASK_TIMEOUT && spent < TEST_LIMIT_MS * 2);
}

/* Adi verilen kontrolu kosar. */
static void	case_check(t_score *score, int no, char *input, char *want)
{
	int	ok;

	if (strcmp(input, "stderr_goes_to_log") == 0)
		ok = check_stderr_goes_to_log();
	else if (strcmp(input, "gives_up_after_three") == 0)
		ok = check_gives_up_after_three();
	else if (strcmp(input, "cancel_wakes_wait") == 0)
		ok = check_cancel_wakes_wait();
	else if (strcmp(input, "backoff_delays_retry") == 0)
		ok = check_backoff_delays_retry();
	else if (strcmp(input, "deaf_write_does_not_hang") == 0)
		ok = check_deaf_write_does_not_hang();
	else if (strcmp(input, "eintr_does_not_extend") == 0)
		ok = check_eintr_does_not_extend();
	else if (strcmp(input, "late_reader_recovers") == 0)
		ok = check_late_reader_recovers();
	else if (strcmp(input, "event_write_marks_dead") == 0)
		ok = check_event_write_marks_dead();
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

	given = "test/cases/naxd.tsv";
	if (argc > 1)
		given = argv[1];
	return (harness_run(given, "naxd testleri", run_case));
}
