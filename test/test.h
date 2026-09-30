/*
** test.h — test kosucularinin paylastigi tipler.
**
** NEDEN VAR:
**   Yapi tanimlari norm geregi basliklarda durur. Sonraki test dosyalari
**   da ayni sayaci kullanacagi icin burasi onlarin da yeri olacak.
*/

#ifndef TEST_H
# define TEST_H

/* Bir test kosusunun sonucu; gecen ve patlayan vaka sayisi. */
typedef struct s_score
{
	int	passed;
	int	failed;
}	t_score;

#endif
