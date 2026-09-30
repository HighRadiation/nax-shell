/*
** exec.h — calistirma tarafinin tipleri ve bildirimleri.
**
** NEDEN AYRI BASLIK:
**   nax.h kabugun ortak cekirdegi, parse.h dilin tarafi. Calistirma
**   ikisinden de ayri bir is: agaci TUKETIR ve surec yonetir. Bildirimleri
**   parse.h'a doldurmak o basligin adini yalan yapardi, nax.h'a koymak ise
**   mumkun degil — nax.h t_cmd'i bilmiyor ve bilmemeli, cunku parse.h
**   nax.h'i dahil ediyor.
**
**   Yerlesik komutlar da bu basliga gelecek.
*/

#ifndef EXEC_H
# define EXEC_H

# include "parse.h"

/*
** Komut cozumleme sonucu.
**
** ONEK NEDEN "RES_":
**   Ilk yazimda sabitler R_OK, R_NOT_FOUND... diye adlandirilmisti ve
**   R_OK <unistd.h> icindeki access() makrosuyla CAKISTI. Makro oldugu
**   icin on islemci path.c'deki her R_OK metnini 4 ile degistirdi, yani
**   R_IS_DIR'in degeriyle. Ortaya gecerli kod ciktigi icin derleyici tek
**   bir uyari vermedi; hata yalnizca davranista gorundu ve bash ile
**   karsilastirma testi yakaladi. Kisa ve genel onekler POSIX makrolariyla
**   cakisir; modul oneki kullanilir.
**
** Ayri ayri tutulmasi gerekiyor cunku her biri farkli bir mesaj ve farkli
** bir cikis kodu uretiyor. Degerler bash olculerek belirlendi:
**
**   RES_OK        cozuldu
**   RES_NOT_FOUND 127, "command not found"        (ciplak ad PATH'te yok)
**   RES_NO_FILE   127, "No such file or directory" (egik cizgili yol yok)
**   RES_NOT_EXEC  126, "Permission denied"
**   RES_IS_DIR    126, "Is a directory"
**
** RES_NOT_FOUND ile RES_NO_FILE ayri, cunku bash iki durumda AYRI mesaj
** veriyor: "boylebirkomutyok" icin command not found, "/yok/bir/yol" icin
** No such file or directory. Cikis kodu ikisinde de 127.
*/
typedef enum e_resolve
{
	RES_OK,
	RES_NOT_FOUND,
	RES_NO_FILE,
	RES_NOT_EXEC,
	RES_IS_DIR
}	t_resolve;

/*
** Boru hattinda bir asamanin dosya tanimlayici baglantilari.
**
** in_fd    : onceki asamanin okuma ucu; yoksa -1
** out_fd   : bu asamanin yazma ucu; yoksa -1
** spare_fd : cocukta KAPATILMASI gereken uc — kendi cikis borusunun okuma
**            ucu; yoksa -1
**
** spare_fd neden var: cocuk kendi cikis borusunun OKUMA ucunu da devralir.
** Kapatilmazsa o fd cocuga, oradan da execve ile calistirilan programa
** sizar. Olculdu: kapatilmadiginda ilk asama /proc/self/fd icinde 0,1,2
** disinda fazladan bir giris goruyor, bash'te gormuyor.
**
** DIKKAT — bu ASILMA sebebi DEGIL: boru EOF'u YAZMA uclari kapandiginda
** gorulur, okuma ucu degil. Asilmaya yol acan sey ANA surecin fds[1]'i
** kapatmamasidir; o zaman yazan taraf hic kapanmis sayilmaz ve okuyan
** asama sonsuza kadar bekler. Ikisi ayri hata; ilk yazimda karistirilmisti
** ve mutasyon denemesi bunu ortaya cikardi.
*/
typedef struct s_stage
{
	int	in_fd;
	int	out_fd;
	int	spare_fd;
}	t_stage;

t_resolve	path_resolve(const char *name, char **out);
const char	*path_reason(t_resolve status);
int			path_code(t_resolve status);

void		ex_warn(const char *message);
void		ex_warn_name(const char *name, const char *reason);
int			redir_apply(const t_xredir *redirs);

void		ex_run(t_shell *sh, const t_cmd *cmds);

#endif
