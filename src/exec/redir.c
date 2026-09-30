/*
** redir.c - yonlendirmeleri cocuk surecte uygular.
**
** NEDEN COCUKTA:
**   Yonlendirme dup2 ile 0 ve 1 numarali tanimlayicilari degistirmek
**   demek. Bunu ANA surecte yapmak kabugun kendi girdi ve ciktisini bozar,
**   sonra kaydedip geri yuklemek gerekir. Cocukta yapmak o soruyu tamamen
**   ortadan kaldiriyor: cocuk zaten execve ile yok olacak, kimse temizlik
**   yapmak zorunda degil.
**
** SIRA KORUNUR:
**   "> a > b" iki dosyayi da acar ve ciktiyi b'ye baglar; a bos olarak
**   olusur. Yani yonlendirme listesinin sirasi gozlenebilir bir davranis,
**   ayristirici de bu yuzden sirayi koruyor. Bash olculerek dogrulandi.
**
** IZINLER:
**   Yeni dosya 0666 ile acilir ve gercek izni umask belirler. Dogrudan
**   0644 yazmak kullanicinin umask'ini yok saymak olurdu.
**
** BASARISIZLIK:
**   open patlarsa sebebi basilir ve cocuk 1 ile cikar. Bash de yonlendirme
**   hatasinda 1 doner (olculdu): olmayan dosyadan okuma, yazilamayan
**   dizine yazma ve dizine yazma denemesi hepsi 1.
*/

#include "nax.h"
#include "exec.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define FILE_MODE 0666

/* Yonlendirme turunun open bayraklarini verir. */
static int	open_flags(t_tok type)
{
	if (type == T_REDIR_IN || type == T_HEREDOC)
		return (O_RDONLY);
	if (type == T_APPEND)
		return (O_WRONLY | O_CREAT | O_APPEND);
	return (O_WRONLY | O_CREAT | O_TRUNC);
}

/* Yonlendirmenin hangi standart tanimlayiciyi degistirdigini verir. */
static int	target_fd(t_tok type)
{
	if (type == T_REDIR_IN || type == T_HEREDOC)
		return (STDIN_FILENO);
	return (STDOUT_FILENO);
}

/* Tek bir yonlendirmeyi uygular; basarisizlikta 0 doner. */
static int	apply_one(const t_xredir *redir)
{
	int	fd;

	fd = open(redir->path, open_flags(redir->type), FILE_MODE);
	if (fd < 0)
	{
		ex_warn_name(redir->path, strerror(errno));
		return (0);
	}
	if (dup2(fd, target_fd(redir->type)) < 0)
	{
		ex_warn_name(redir->path, strerror(errno));
		close(fd);
		return (0);
	}
	close(fd);
	return (1);
}

/* Tum yonlendirmeleri yazildiklari sirada uygular. */
int	redir_apply(const t_xredir *redirs)
{
	while (redirs != NULL)
	{
		if (apply_one(redirs) == 0)
			return (0);
		redirs = redirs->next;
	}
	return (1);
}
