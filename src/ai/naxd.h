/*
** naxd.h - yardimci surecin yasam dongusu ve onunla konusma.
**
** NEDEN proto.h'TAN AYRI:
**   proto.h bicimi ve satir okumayi anlatiyor; burasi SUREC yonetimi.
**   Biri degisirken digerinin degismemesi normal: bicim sabit kalirken
**   gozetim kurallari degisebilir, tersi de olur.
**
** TEK BEKLEYEN ISTEK:
**   Ayni anda yalnizca bir kullanici istegi havada olur. Kabuk zaten
**   cevabi bekliyor, yani bu kural C tarafindaki tum eszamanlilik
**   ihtiyacini ortadan kaldiriyor. id artan bir sayi; eski ya da
**   taninmayan id tasiyan yanitlar sessizce atilir.
**
** BEKLERKEN NE GOZLENIR:
**   Iki tanimlayici birden: yardimci surecin ciktisi ve KENDINE BORU.
**   Kesme isleyicisi o boruya bir bayt yazar - sinyal baglaminda guvenli
**   tek is bu. Boylece kullanici Ctrl-C ile beklemeyi kesebiliyor ve
**   kabuk sinyal baglaminda hicbir riskli is yapmiyor.
**
** OLUM KABUGU GOTURMEZ:
**   SIGPIPE yok sayili oldugu icin olmus surece yazmak EPIPE dondurur.
**   Yeniden dogma BEKLEMEDEN yapilmaz: bir sonraki istegin zamani
**   geldiginde denenir, cunku kabugun icinde saniyelerce uyumak
**   kullaniciyi bekletmek olurdu.
*/

#ifndef NAXD_H
# define NAXD_H

# include <sys/types.h>
# include "proto.h"

/*
** Yardimci surecin durumu.
**
** AI_OFF      kapali: hic baslatilmadi ya da ust uste basarisiz oldu
** AI_STARTING surec var, READY bekleniyor
** AI_READY    konusmaya hazir
*/
typedef enum e_aistate
{
	AI_OFF,
	AI_STARTING,
	AI_READY
}	t_aistate;

/*
** Bir istegin sonucu.
**
** ASK_CONTINUE icsel: "henuz karar yok, beklemeye devam". Disa
** sizmasinin sebebi tamponu bosaltan islevin ayni degeri kullanmasi;
** ayri bir tip uretmek iki enum arasinda cevirme yazdirirdi.
**
** ASK_PROTOCOL ile ASK_DEAD AYRI: ikisi de sureci yeniden baslatmayi
** gerektiriyor ama birincisi karsi tarafin bozuk konustugunu, ikincisi
** hic konusmadigini soyluyor. Gunluge farkli yazilmalari gerekiyor.
*/
typedef enum e_askst
{
	ASK_CONTINUE,
	ASK_OK,
	ASK_NEED,
	ASK_ERR,
	ASK_TIMEOUT,
	ASK_CANCEL,
	ASK_DEAD,
	ASK_PROTOCOL
}	t_askst;

/* Bekleme uzadiginda bir kez cagrilir; gostergeyi basan taraf kabuk. */
typedef void	(*t_tick_fn)(void);

# define NAXD_TICK_MS 4000
# define NAXD_LIMIT_MS 15000
# define NAXD_FAIL_MAX 3
# define NAXD_FAIL_WINDOW_MS 60000

/*
** Yardimci surecle baglanti.
**
** wake_rd ve wake_wr kendine boru. Yazma ucu sinyal isleyicisine
** veriliyor; okuma ucu beklemede gozlenen ikinci tanimlayici.
**
** nogo, AI'in tamamen kapali oldugu dizinlerin listesi. Yapilandirmada
** duruyor ama karari kabuk veriyor: "hic gonderme" karari GONDEREN
** tarafta olmak zorunda, yoksa veri karsi tarafa ulasmis olurdu.
**
** has_key, READY kaydindaki "key" alanindan gelir. Kabuk bunu bilmek
** zorunda: anahtar yoksa istek gondermek bos bir tur demek ve kullaniciya
** oturumda BIR KEZ sebebi soylenmeli. Anahtarsiz durum bir hata hali
** degil, desteklenen bir calisma bicimi.
**
** tick_ms ve limit_ms YAPIDA, sabit degil: testler on bes saniye
** beklemek zorunda kalmamali ve ayni degerler ileride yapilandirmadan
** gelecek. Varsayilanlari asagidaki sabitler.
**
** fails ve first_fail_ms "bir dakikada uc basarisizlik" kuralini tutuyor.
** next_try_ms ise bir sonraki yeniden dogma denemesinin en erken zamani:
** kabuk icinde uyumak yerine zamani kaydedip istegi o ana kadar
** reddetmek, kullaniciyi bekletmemenin tek yolu.
*/
typedef struct s_naxd
{
	pid_t		pid;
	int			in_fd;
	t_reader	out;
	int			wake_rd;
	int			wake_wr;
	long		next_id;
	t_aistate	state;
	int			fails;
	int			tries;
	int			warned;
	int			has_key;
	char		*nogo;
	long		first_fail_ms;
	long		next_try_ms;
	long		tick_ms;
	long		limit_ms;
	char		*const *argv;
	const char	*log_path;
}	t_naxd;

long		naxd_now_ms(void);
void		naxd_init(t_naxd *nx, char *const *argv, const char *log_path);
int			naxd_open(t_naxd *nx);
void		naxd_close(t_naxd *nx);
int			naxd_alive(const t_naxd *nx);
int			naxd_write(t_naxd *nx, const char *line);
int			naxd_send(t_naxd *nx, t_ftype type, const t_pair *fields,
				size_t n);
t_askst		naxd_ask(t_naxd *nx, t_ftype type, const t_pair *fields,
				size_t n, t_frame *reply, t_tick_fn tick);
t_askst		naxd_wait(t_naxd *nx, long id, t_frame *reply, t_tick_fn tick);
long		naxd_last_id(const t_naxd *nx);
const char	*naxd_ask_name(t_askst state);

#endif
