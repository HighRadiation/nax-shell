/*
** nax - ortak tipler ve butun modullerin paylastigi bildirimler.
**
** NEDEN TEK BASLIK:
**   Her .c dosyasi icin ayri bir .h yazmak bu boyutta dosya sayisini
**   gereksiz yere ikiye katliyor. Paylasilan tipler burada toplanir;
**   alan bazli prototipler ise alan basliklarina (parse.h, ai.h) gider.
**
** ISIMLENDIRME:
**   Dosya, fonksiyon ve degisken adlari Ingilizce; yorumlar Turkce.
**   Her fonksiyonun ustunde ne yaptigini soyleyen bir yorum bulunur,
**   fonksiyon govdesinde yorum bulunmaz. Ayrintisi docs/NORMS.md'de.
*/

#ifndef NAX_H
# define NAX_H

# include <stddef.h>

# define NAX_NAME "nax"
# define NAX_VERSION "0.1.0"

/*
** Buyuyebilen metin tamponu.
**
** Sozcuk ayirici, kanonik yazdirici ve genisletme ucu de karakter
** karakter metin biriktirir; bu yuzden tampon ortak tip olarak burada
** durur. Sahiplik kurallari src/buf.c dosyasinin basinda yazili.
*/
typedef struct s_buf
{
	char	*data;
	size_t	len;
	size_t	cap;
}	t_buf;

/*
** Kabugun oturum boyu yasayan durumu.
**
** last_status : en son calistirilan isin cikis kodu; $? bunu okuyacak
** hist_path   : readline gecmisinin saklandigi dosyanin tam yolu
** interactive : stdin bir terminal mi; betik modunda AI kapali kalir
** exiting     : okuma dongusunun bu turdan sonra bitmesi isteniyor
*/
typedef struct s_shell
{
	int		last_status;
	char	*hist_path;
	int		interactive;
	int		exiting;
}	t_shell;

void	buf_init(t_buf *buf);
int		buf_push(t_buf *buf, char c);
int		buf_push_str(t_buf *buf, const char *s);
int		buf_empty(const t_buf *buf);
char	*buf_take(t_buf *buf);
void	buf_free(t_buf *buf);

void	ln_setup(void);
char	*ln_hist_path(void);
void	ln_hist_load(const char *path);
void	ln_hist_save(const char *path);
char	*ln_read(const t_shell *sh);

void	sig_setup_interactive(void);
void	sig_reset_child(void);
int		sig_take_interrupt(void);

#endif
