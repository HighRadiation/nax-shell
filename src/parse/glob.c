/*
** glob.c - dosya adi genisletmesi.
**
** SIRA: genisletme, ALAN AYIRMA, sonra bu adim. Bash de bu sirada
** yapiyor ve onemi su: "X='a b'; echo $X*" satirinda once iki alan
** olusuyor, desen her birine ayri ayri uygulaniyor.
**
** ESLESME YOKSA DESEN OLDUGU GIBI KALIR:
**   "echo yok*.zzz" satiri aynen basilir. Bash'in varsayilani bu ve
**   olculdu. Alternatifi alani silmek olurdu ki o, kullanicinin yazdigi
**   seyi sessizce yok etmek demek.
**
** GIZLI DOSYALAR ESLESMEZ:
**   Bir bilesen "." ile BASLAYAN bir adla eslesmek icin desenin de "."
**   ile baslamasi gerekiyor. Olculdu: "echo *" ciktisinda ".gizli" yok
**   ama "echo .gi*" onu buluyor.
**
**   KURAL MASKEYE BAKMIYOR: nokta hicbir zaman desen karakteri degil,
**   yani tirnakli olup olmamasi fark etmez. Ilk yazim tirnakli olmasini
**   sarti kosuyordu ve ".gi*" gizli dosyayi bulamiyordu.
**
** SONUC SIRALI:
**   Dizin okuma sirasi dosya sistemine gore degisir ve ayni komutun ayni
**   dizinde farkli siralarda cikti vermesi kabul edilemez. Bash de
**   siraliyor.
*/

#include "nax.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/*
** Girdi "." ya da ".." mi.
**
** Bu ikisi hicbir desenle eslesmez. Olculdu: "echo .*" bash'te yalnizca
** gercek gizli dosyalari veriyor. Atilmazlarsa "." ve ".." her gizli
** dosya listesinin basina gelir ve cikti ise yaramaz hale gelir.
*/
static int	is_dot_entry(const char *name)
{
	if (name[0] != '.')
		return (0);
	return (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

/* Alanda desen olarak islev goren bir karakter var mi. */
static int	has_pattern(const t_field *field)
{
	size_t	i;

	i = 0;
	while (field->text[i] != '\0' && field->mask[i] != '\0')
	{
		if (field->mask[i] == '0' && (field->text[i] == '*'
				|| field->text[i] == '?' || field->text[i] == '['))
			return (1);
		i++;
	}
	return (0);
}

/* Yeni bir alan kaydi uretir; maskesi harfi harfine olur. */
static t_field	*field_new(const char *text)
{
	t_field	*field;
	size_t	len;

	field = malloc(sizeof(*field));
	if (field == NULL)
		return (NULL);
	len = strlen(text);
	field->text = strdup(text);
	field->mask = malloc(len + 1);
	field->next = NULL;
	if (field->text == NULL || field->mask == NULL)
	{
		free(field->text);
		free(field->mask);
		free(field);
		return (NULL);
	}
	memset(field->mask, '1', len);
	field->mask[len] = '\0';
	return (field);
}

/* Listenin sonuna ekler ve yeni kuyrugu dondurur. */
static t_field	*field_link(t_field **head, t_field *tail, t_field *node)
{
	if (tail == NULL)
		*head = node;
	else
		tail->next = node;
	return (node);
}

/* Iki yolu bolu isaretiyle birlestirir; cagiran serbest birakir. */
static char	*join_path(const char *base, const char *name)
{
	t_buf	buf;

	buf_init(&buf);
	if (buf_push_str(&buf, base) == 0)
		return (buf_free(&buf), NULL);
	if (*base != '\0' && base[strlen(base) - 1] != '/')
		if (buf_push(&buf, '/') == 0)
			return (buf_free(&buf), NULL);
	if (buf_push_str(&buf, name) == 0)
		return (buf_free(&buf), NULL);
	if (buf.data == NULL)
		return (strdup(""));
	return (buf_take(&buf));
}

/*
** Bir dizini tarayip desene uyan adlari listeye ekler.
**
** Taban bos dize ise calisma dizini okunur ama uretilen yola bos taban
** eklenmez; boylece "*.c" sonucu "./a.c" degil "a.c" oluyor.
*/
static int	scan_dir(const char *base, const char *pat, const char *mask,
		t_field **head)
{
	DIR				*dp;
	struct dirent	*entry;
	t_field			*tail;
	char			*path;

	dp = opendir(*base == '\0' ? "." : base);
	if (dp == NULL)
		return (1);
	tail = *head;
	while (tail != NULL && tail->next != NULL)
		tail = tail->next;
	entry = readdir(dp);
	while (entry != NULL)
	{
		if (is_dot_entry(entry->d_name) == 0
			&& !(entry->d_name[0] == '.' && pat[0] != '.')
			&& glob_match(pat, mask, entry->d_name))
		{
			path = join_path(base, entry->d_name);
			if (path == NULL)
				return (closedir(dp), 0);
			tail = field_link(head, tail, field_new(path));
			free(path);
			if (tail == NULL)
				return (closedir(dp), 0);
		}
		entry = readdir(dp);
	}
	closedir(dp);
	return (1);
}

/* Yol var mi; kirik baglantilar da var sayilir. */
static int	path_exists(const char *path)
{
	struct stat	info;

	if (*path == '\0')
		return (0);
	return (lstat(path, &info) == 0);
}

/* Listeyi serbest birakir. */
static void	list_free(t_field *list)
{
	t_field	*next;

	while (list != NULL)
	{
		next = list->next;
		free(list->text);
		free(list->mask);
		free(list);
		list = next;
	}
}

/*
** Bir bileseni tum tabanlara uygular ve yeni taban listesini dondurur.
**
** Bilesen desen iceriyorsa dizin taranir; icermiyorsa yola eklenir.
** Eklenen yolun var olup olmadigi BURADA denetlenmiyor, en sonda tek bir
** suzgecle yapiliyor. Onemi su: yildiz ile baslayip sabit bir ad ile
** biten desenler (ornegin yildiz, bolu, "c.c") olmayan birlesimler
** uretir ve hepsi tek yerde temizleniyor.
*/
static t_field	*apply_part(t_field *bases, const char *pat, const char *mask)
{
	t_field	*out;
	t_field	*tail;
	t_field	*base;
	char	*path;
	int		pattern;

	out = NULL;
	tail = NULL;
	pattern = (strpbrk(pat, "*?[") != NULL && strchr(mask, '0') != NULL);
	base = bases;
	while (base != NULL)
	{
		if (pattern)
		{
			if (scan_dir(base->text, pat, mask, &out) == 0)
				return (list_free(out), list_free(bases), NULL);
			tail = out;
			while (tail != NULL && tail->next != NULL)
				tail = tail->next;
		}
		else
		{
			path = join_path(base->text, pat);
			if (path == NULL)
				return (list_free(out), list_free(bases), NULL);
			tail = field_link(&out, tail, field_new(path));
			free(path);
			if (tail == NULL)
				return (list_free(out), list_free(bases), NULL);
		}
		base = base->next;
	}
	list_free(bases);
	return (out);
}

/* Metinleri karsilastirir; siralama icin. */
static int	cmp_text(const void *a, const void *b)
{
	return (strcmp(*(const char **)a, *(const char **)b));
}

/*
** Listeyi ada gore siralar; basarida 1.
**
** NEDEN SIRALI: dizin okuma sirasi dosya sistemine gore degisiyor ve ayni
** komutun ayni dizinde farkli siralarda cikti vermesi kabul edilemez.
*/
static int	sort_list(t_field *list)
{
	char	**names;
	size_t	n;
	size_t	i;
	t_field	*at;

	n = 0;
	at = list;
	while (at != NULL && ++n)
		at = at->next;
	if (n < 2)
		return (1);
	names = malloc(n * sizeof(*names));
	if (names == NULL)
		return (0);
	i = 0;
	at = list;
	while (at != NULL)
	{
		names[i++] = at->text;
		at = at->next;
	}
	qsort(names, n, sizeof(*names), cmp_text);
	i = 0;
	at = list;
	while (at != NULL)
	{
		at->text = names[i++];
		at = at->next;
	}
	free(names);
	return (1);
}

/* Var olmayan yollari listeden cikarir. */
static t_field	*keep_existing(t_field *list)
{
	t_field	*head;
	t_field	*tail;
	t_field	*next;

	head = NULL;
	tail = NULL;
	while (list != NULL)
	{
		next = list->next;
		list->next = NULL;
		if (path_exists(list->text))
			tail = field_link(&head, tail, list);
		else
			list_free(list);
		list = next;
	}
	return (head);
}

/*
** Bir alani bilesen bilesen genisletir; eslesme yoksa NULL doner.
**
** Mutlak yol "/" tabanindan baslar. Bos bilesenler atlanir, yani "a//b"
** ile "a/b" ayni sonucu verir; bu bilincli bir basitlestirme ve bash'in
** davranisiyla pratikte ayni.
*/
static t_field	*glob_one(const t_field *field)
{
	t_field	*bases;
	char	*text;
	char	*mask;
	size_t	i;
	size_t	start;

	bases = field_new(field->text[0] == '/' ? "/" : "");
	text = strdup(field->text);
	mask = strdup(field->mask);
	if (bases == NULL || text == NULL || mask == NULL)
		return (list_free(bases), free(text), free(mask), NULL);
	i = 0;
	start = 0;
	while (bases != NULL)
	{
		while (text[i] != '\0' && text[i] != '/')
			i++;
		if (text[i] == '/')
			text[i++] = '\0';
		if (text[start] != '\0')
			bases = apply_part(bases, text + start, mask + start);
		if (text[i] == '\0' && (i == 0 || text[i - 1] != '\0'))
			break ;
		start = i;
	}
	free(text);
	free(mask);
	return (keep_existing(bases));
}

/*
** Alan listesine dosya adi genisletmesi uygular.
**
** Verilen liste DEVRALINIR ve yerine yenisi dondurulur. Eslesme bulunan
** alan sonuclariyla, bulunmayan alan OLDUGU GIBI korunur: bash'in
** varsayilani bu ve alani silmek kullanicinin yazdigini sessizce yok
** etmek olurdu.
*/
t_field	*glob_fields(t_field *fields)
{
	t_field	*head;
	t_field	*tail;
	t_field	*next;
	t_field	*found;

	head = NULL;
	tail = NULL;
	while (fields != NULL)
	{
		next = fields->next;
		fields->next = NULL;
		found = NULL;
		if (has_pattern(fields))
			found = glob_one(fields);
		if (found == NULL)
			tail = field_link(&head, tail, fields);
		else
		{
			list_free(fields);
			sort_list(found);
			tail = field_link(&head, tail, found);
			while (tail != NULL && tail->next != NULL)
				tail = tail->next;
		}
		fields = next;
	}
	return (head);
}
