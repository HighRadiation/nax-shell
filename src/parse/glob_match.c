/*
** glob_match.c - tek bir yol bileseni icin desen eslesmesi.
**
** MASKE OLMADAN BU IS YAPILAMAZ:
**   Hangi yildizin desen oldugu ancak maskeden bilinir. "echo "a"*.c"
**   satirinda ayni sozcukte hem harf hem desen var; sozcuk bazinda bir
**   bayrak bunu yanlis yapardi. Maske her karakter icin "1" harf, "0"
**   desen diyor.
**
** DESTEKLENEN DESENLER:
**   *        herhangi bir dizi (bu bilesen icinde)
**   ?        tek karakter
**   [abc]    kume; [a-z] aralik, [!abc] ve [^abc] olumsuzlama
**
**   Bilesen sinirlari cagiran tarafindan ayrildigi icin "*" burada "/"
**   karakterini gecmek zorunda degil; o sorun hic ortaya cikmiyor.
**
** YILDIZ ICIN OZYINELEME:
**   "*" sonrasini her olasi konumda denemek gerekiyor. Dongu ile yazmak
**   mumkun ama geri izleme durumunu elle tutmak demek; ozyineleme bu
**   islevde iki satir ve okunur kaliyor. Bilesen uzunluklari dosya adi
**   olcegindedir, derinlik sorun degil.
*/

#include "nax.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>

/*
** Kume desenini okur; eslesirse 1 doner ve *at kume sonrasina gecer.
**
** Kapanmamis kose parantez DESEN DEGIL harf sayilir: bash da boyle
** davraniyor ve "echo [abc" satiri oldugu gibi basiliyor.
*/
static int	match_class(const char *pattern, const char *mask, size_t *at,
		char c)
{
	size_t	i;
	int		negate;
	int		hit;

	i = *at + 1;
	negate = 0;
	if (pattern[i] == '!' || pattern[i] == '^')
	{
		negate = 1;
		i++;
	}
	hit = 0;
	while (pattern[i] != '\0' && !(pattern[i] == ']' && mask[i] == '0'))
	{
		if (pattern[i + 1] == '-' && pattern[i + 2] != '\0'
			&& pattern[i + 2] != ']')
		{
			if (c >= pattern[i] && c <= pattern[i + 2])
				hit = 1;
			i += 3;
			continue ;
		}
		if (pattern[i] == c)
			hit = 1;
		i++;
	}
	if (pattern[i] != ']')
		return (-1);
	*at = i + 1;
	return (hit != negate);
}

/* Konumdaki karakter desen anlami tasiyor mu. */
static int	is_meta(char c, char m)
{
	if (m != '0')
		return (0);
	return (c == '*' || c == '?' || c == '[');
}

/*
** Deseni adla karsilastirir; eslesirse 1.
**
** Yildiz halinde kalan deseni her olasi konumda deniyor. Bos eslesme de
** gecerli, o yuzden dongu adin sonunu da kapsiyor.
*/
int	glob_match(const char *pattern, const char *mask, const char *name)
{
	size_t	i;
	int		ok;

	i = 0;
	if (is_meta(pattern[0], mask[0]) && pattern[0] == '*')
	{
		while (1)
		{
			if (glob_match(pattern + 1, mask + 1, name))
				return (1);
			if (*name == '\0')
				return (0);
			name++;
		}
	}
	if (pattern[0] == '\0')
		return (*name == '\0');
	if (*name == '\0')
		return (0);
	if (is_meta(pattern[0], mask[0]) && pattern[0] == '?')
		return (glob_match(pattern + 1, mask + 1, name + 1));
	if (is_meta(pattern[0], mask[0]) && pattern[0] == '[')
	{
		ok = match_class(pattern, mask, &i, *name);
		if (ok >= 0)
		{
			if (ok == 0)
				return (0);
			return (glob_match(pattern + i, mask + i, name + 1));
		}
	}
	if (pattern[0] != *name)
		return (0);
	return (glob_match(pattern + 1, mask + 1, name + 1));
}
