/*
** path.c - komut adini calistirilabilir bir yola cozer.
**
** IKI AYRI KURAL, cunku bash de boyle davraniyor (olculdu):
**
**   Ad "/" iceriyorsa PATH'e HIC bakilmaz, dogrudan o yol denenir ve
**   basarisizligin sebebi ayirt edilir:
**     yok        -> 127, "No such file or directory"
**     dizin      -> 126, "Is a directory"
**     yetki yok  -> 126, "Permission denied"
**
**   Ad "/" icermiyorsa PATH dizinleri sirayla taranir ve YALNIZCA
**   calistirilabilir dosyalar sayilir. Hicbiri bulunmazsa 127,
**   "command not found". Olculen sonuc: PATH icinde ayni adda
**   calistirilamayan bir dosya bulunsa bile bash 126 degil 127 veriyor.
**
** ONBELLEK YOK:
**   Cozumleme komut basina bir kez oluyor ve alti dizini access() ile
**   taramak mikrosaniyeler suruyor. PATH onbellegi siniflandirici icin
**   anlamli olacak - orada karar milisaniyenin altinda kalmak zorunda -
**   ve o zaman eklenecek. Simdi eklemek olculmemis bir iyilestirme olurdu.
*/

#include "nax.h"
#include "exec.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Cozumleme sonucunun kullaniciya gosterilecek sebebini dondurur. */
const char	*path_reason(t_resolve status)
{
	if (status == RES_IS_DIR)
		return ("Is a directory");
	if (status == RES_NOT_EXEC)
		return ("Permission denied");
	if (status == RES_NO_FILE)
		return ("No such file or directory");
	return ("command not found");
}

/* Cozumleme sonucunun cikis kodunu dondurur. */
int	path_code(t_resolve status)
{
	if (status == RES_IS_DIR || status == RES_NOT_EXEC)
		return (126);
	return (127);
}

/* Verilen yolun neden calistirilamadigini ayirt eder. */
static t_resolve	classify_path(const char *path)
{
	struct stat	info;

	if (stat(path, &info) != 0)
		return (RES_NO_FILE);
	if (S_ISDIR(info.st_mode))
		return (RES_IS_DIR);
	if (access(path, X_OK) != 0)
		return (RES_NOT_EXEC);
	return (RES_OK);
}

/* Dizin ve ad birlestirip yeni bir yol uretir; cagiran serbest birakir. */
static char	*join_path(const char *dir, size_t dir_len, const char *name)
{
	char	*path;
	size_t	name_len;

	name_len = strlen(name);
	path = malloc(dir_len + name_len + 2);
	if (path == NULL)
		return (NULL);
	memcpy(path, dir, dir_len);
	path[dir_len] = '/';
	memcpy(path + dir_len + 1, name, name_len + 1);
	return (path);
}

/* PATH icindeki tek bir dizinde adi arar; bulursa yolu out'a yazar. */
static int	try_dir(const char *dir, size_t len, const char *name, char **out)
{
	char	*path;

	if (len == 0)
		return (0);
	path = join_path(dir, len, name);
	if (path == NULL)
		return (0);
	if (classify_path(path) == RES_OK)
	{
		*out = path;
		return (1);
	}
	free(path);
	return (0);
}

/* PATH dizinlerini sirayla tarar; yalnizca calistirilabilir olan sayilir. */
static t_resolve	search_path(const char *name, char **out)
{
	const char	*list;
	const char	*colon;

	list = getenv("PATH");
	if (list == NULL || *list == '\0')
		return (RES_NOT_FOUND);
	while (1)
	{
		colon = strchr(list, ':');
		if (colon == NULL)
			colon = list + strlen(list);
		if (try_dir(list, (size_t)(colon - list), name, out))
			return (RES_OK);
		if (*colon == '\0')
			return (RES_NOT_FOUND);
		list = colon + 1;
	}
}

/*
** Ad bir komuta cozuluyor mu; yalnizca evet/hayir gerektiginde kullanilir.
**
** Siniflandirici bunu her satirda cagiriyor ama yola ihtiyaci yok. Ayri
** bir islev olmasinin sebebi cagiranin donen yolu birakmayi unutmamasi;
** onbellek gelene kadar ic tarafta yine ayirma var.
*/
int	path_is_command(const char *name)
{
	char	*path;

	if (path_resolve(name, &path) != RES_OK)
		return (0);
	free(path);
	return (1);
}

/*
** Komut adini calistirilabilir bir yola cozer.
**
** RES_OK donerse out yeni ayrilmis bir yol tutar ve cagiran birakir;
** diger sonuclarda out'a dokunulmaz.
*/
t_resolve	path_resolve(const char *name, char **out)
{
	t_resolve	status;

	if (*name == '\0')
		return (RES_NOT_FOUND);
	if (strchr(name, '/') != NULL)
	{
		status = classify_path(name);
		if (status != RES_OK)
			return (status);
		*out = strdup(name);
		if (*out == NULL)
			return (RES_NO_FILE);
		return (RES_OK);
	}
	return (search_path(name, out));
}
