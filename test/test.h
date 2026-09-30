/*
** test.h — tablo tabanli test kosucularinin paylastigi tipler ve iskele.
**
** NEDEN VAR:
**   Ucuncu test dosyasi yazilirken vaka dosyasini okuma, kacis cozme ve
**   ozet basma kodunun uc kopyasi olusacakti. Ortak olani buraya almak
**   modulerlik kuralinin geregi. Yapi tanimlari da norm geregi basliklarda
**   durur.
*/

#ifndef TEST_H
# define TEST_H

# define TEST_GREEN "\033[0;32m"
# define TEST_RED "\033[0;31m"
# define TEST_OFF "\033[0m"

/* Bir test kosusunun sonucu; gecen ve patlayan vaka sayisi. */
typedef struct s_score
{
	int	passed;
	int	failed;
}	t_score;

/*
** Tek bir vakayi kosan islev.
**
** input ve want kacislari COZULMUS halde gelir ve degistirilebilir; her
** ikisi de iskelenin sahip oldugu tampondadir, cagrilan serbest birakmaz.
*/
typedef void	(*t_case_fn)(t_score *score, int no, char *input, char *want);

void	report_fail(t_score *score, int no, const char *input,
			const char *want, const char *got);
int		harness_run(const char *path, const char *title, t_case_fn fn);

#endif
