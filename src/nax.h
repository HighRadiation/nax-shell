/*
** nax — ortak tipler ve butun modullerin paylastigi bildirimler.
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

# define NAX_NAME "nax"
# define NAX_VERSION "0.1.0"

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

void	ln_setup(void);
char	*ln_hist_path(void);
void	ln_hist_load(const char *path);
void	ln_hist_save(const char *path);
char	*ln_read(const t_shell *sh);

void	sig_setup_interactive(void);
int		sig_take_interrupt(void);

#endif
