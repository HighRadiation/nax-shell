/*
** expand.c - sozcukleri son hallerine, yani alanlara cevirir.
**
** SIRA (basitlestirilmis POSIX sirasi):
**   1. ~ genisletmesi   yalnizca sozcugun basindaki tirnaksiz parcada
**   2. $ genisletmesi   $NAME, ${NAME}, $?
**   3. alan ayirma      YALNIZCA tirnaksiz genisletmeden gelen metinde
**   4. tirnak kaldirma  sozcuk ayirici zaten yapti, burada is kalmadi
**
** TIRNAK KALDIRMANIN BURADA OLMAMASI:
**   Sozcuk ayirici tirnaklari silip yerine parca kipi birakti. Bu yuzden
**   burada tirnak ayristirmak gerekmiyor; her parca ne yapilacagini kendi
**   soyluyor. Sozlesme docs/ARCHITECTURE.md icinde.
**
** ALAN AYIRMANIN INCE NOKTASI:
**   Harfi harfine yazilmis metin YENIDEN BOLUNMEZ, cunku sozcuk ayirici
**   onu zaten bosluklardan bolmustu. Yalnizca genisletmeden GELEN metin
**   bolunur. X="a b" iken:
**     echo $X    -> iki alan
**     echo "$X"  -> tek alan
**   Ayrimi parca kipi tasiyor: Q_NONE bolunur, Q_DOUBLE bolunmez,
**   Q_SINGLE hic genisletilmez.
**
** BOS ALAN KURALI:
**   "emit" bayragi SOZCUK basina degil ALAN basina tutulur ve her alan
**   uretildiginde sifirlanir. Bayragi kuran sey harfi harfine ya da
**   tirnakli icerik; genisletmeden gelen karakterler kurmaz. Boylece:
**     echo $YOK    -> 0 alan
**     echo "$YOK"  -> 1 bos alan
**     TSP="a "; echo $TSP""  -> 2 alan, [a] ve bos olan
**   Ucu de bash'in davranisi olculerek dogrulandi; sozcuk basina bir
**   bayrak ucuncusunu kaybediyordu.
*/

#include "nax.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>

#define NAME_MAX_LEN 256

/* Karakter alan ayirici bir bosluk mu. */
static int	is_ifs(char c)
{
	return (c == ' ' || c == '\t' || c == '\n');
}

/* Karakter degisken adinin ilk harfi olabilir mi. */
static int	is_name_start(char c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

/* Karakter degisken adinin devami olabilir mi. */
static int	is_name_char(char c)
{
	return (is_name_start(c) || (c >= '0' && c <= '9'));
}

/* Hatayi kaydeder ve her zaman 0 doner; cagiran dogrudan dondurebilir. */
static int	fail(t_expander *exp, const char *message)
{
	exp->err->message = message;
	exp->err->at = exp->word;
	return (0);
}

/* Verilen metni sahiplenip yeni bir alan olarak listeye ekler. */
static int	field_add(t_expander *exp, char *text, char *mask)
{
	t_field	*field;

	if (text == NULL || mask == NULL)
		return (free(text), free(mask), 0);
	field = malloc(sizeof(*field));
	if (field == NULL)
	{
		free(text);
		free(mask);
		return (0);
	}
	field->text = text;
	field->mask = mask;
	field->next = NULL;
	if (exp->tail == NULL)
		exp->head = field;
	else
		exp->tail->next = field;
	exp->tail = field;
	exp->emit = 0;
	return (1);
}

/* Tamponda metin varsa onu alana cevirir; bayragi sifirlar. */
static int	field_flush(t_expander *exp)
{
	if (buf_empty(&exp->buf))
		return (1);
	return (field_add(exp, buf_take(&exp->buf), buf_take(&exp->mask)));
}

/*
** Tek karakteri tampona ve yanina maske bitini yazar.
**
** Maske buf ile AYNI uzunlukta kalmak zorunda; bu yuzden tamponlara
** yazan tek yol bu islev. Ikisini ayri ayri doldurmak, bir yerde
** unutuldugunda maskeyi metinle kaydirip tum desenleri bozardi.
*/
static int	push_one(t_expander *exp, char c)
{
	if (buf_push(&exp->buf, c) == 0)
		return (0);
	return (buf_push(&exp->mask, (char)('0' + (exp->literal != 0))));
}

/* Metni oldugu gibi tampona ekler; alan ayirmaya tabi tutmaz. */
static int	append_plain(t_expander *exp, const char *s)
{
	while (*s != '\0')
	{
		if (push_one(exp, *s) == 0)
			return (0);
		s++;
	}
	return (1);
}

/* Metni tampona eklerken bosluk dizilerinden alanlara boler. */
static int	append_split(t_expander *exp, const char *s)
{
	while (*s != '\0')
	{
		if (is_ifs(*s))
		{
			if (field_flush(exp) == 0)
				return (0);
			while (is_ifs(*s))
				s++;
			continue ;
		}
		if (push_one(exp, *s) == 0)
			return (0);
		s++;
	}
	return (1);
}

/* Degeri, tirnak durumuna gore bolerek ya da oldugu gibi ekler. */
static int	append_value(t_expander *exp, const char *value, int split)
{
	if (value == NULL)
		return (1);
	if (split)
		return (append_split(exp, value));
	return (append_plain(exp, value));
}

/* Tam sayiyi tampona yazar; $? icin gerekli. */
static int	append_number(t_expander *exp, int value)
{
	char	digits[12];
	int		i;

	i = (int)sizeof(digits) - 1;
	digits[i] = '\0';
	if (value == 0)
	{
		i--;
		digits[i] = '0';
	}
	while (value > 0)
	{
		i--;
		digits[i] = (char)('0' + value % 10);
		value /= 10;
	}
	return (append_plain(exp, digits + i));
}

/* Metnin basindan itibaren gecerli bir degisken adi mi. */
static int	is_valid_name(const char *s, size_t len)
{
	size_t	i;

	if (len == 0 || is_name_start(s[0]) == 0)
		return (0);
	i = 1;
	while (i < len)
	{
		if (is_name_char(s[i]) == 0)
			return (0);
		i++;
	}
	return (1);
}

/*
** ${NAME} bicimini isler.
**
** Kapanmamis parantez, bos ad ve GECERSIZ AD hatadir. Ad dogrulamasi
** onemli: "${TV:-varsayilan}" gibi desteklenmeyen bir bicim dogrulama
** olmadan "TV:-varsayilan" adini arar, bulamaz ve SESSIZCE bosa genisler.
** Sessiz yanlis cevap hatadan kotudur. Desteklenmeyen parametre bicimleri
** docs/FINDINGS.md icinde kayitli.
*/
static int	expand_braced(t_expander *exp, const char *s, size_t *i, int split)
{
	size_t	start;
	size_t	len;
	char	name[NAME_MAX_LEN];

	start = *i + 2;
	len = 0;
	while (s[start + len] != '\0' && s[start + len] != '}')
		len++;
	if (s[start + len] != '}' || len >= sizeof(name))
		return (fail(exp, "hatali degisken yazimi"));
	if (is_valid_name(s + start, len) == 0)
		return (fail(exp, "hatali degisken yazimi"));
	memcpy(name, s + start, len);
	name[len] = '\0';
	*i = start + len + 1;
	return (append_value(exp, getenv(name), split));
}

/* $NAME bicimini isler. */
static int	expand_named(t_expander *exp, const char *s, size_t *i, int split)
{
	size_t	len;
	char	name[NAME_MAX_LEN];

	len = 0;
	while (is_name_char(s[*i + 1 + len]))
		len++;
	if (len >= sizeof(name))
		return (fail(exp, "degisken adi cok uzun"));
	memcpy(name, s + *i + 1, len);
	name[len] = '\0';
	*i += 1 + len;
	return (append_value(exp, getenv(name), split));
}

/*
** Dolar isaretinden sonrasini isler.
**
** Desteklenenler: $NAME, ${NAME}, $? ve $ ardindan rakam. Rakam bicimi
** bos genisler, cunku bu kabukta konumsal parametre yok; bash de
** parametresiz cagrildiginda ayni sonucu veriyor. Bunlarin disinda dolar
** isareti harfi harfine kalir ve harfi harfine icerik sayilir.
*/
static int	expand_dollar(t_expander *exp, const char *s, size_t *i, int split)
{
	char	next;

	next = s[*i + 1];
	if (next == '{')
		return (expand_braced(exp, s, i, split));
	if (next == '?')
	{
		*i += 2;
		return (append_number(exp, exp->sh->last_status));
	}
	if (next >= '0' && next <= '9')
	{
		*i += 2;
		return (1);
	}
	if (is_name_start(next))
		return (expand_named(exp, s, i, split));
	*i += 1;
	exp->emit = 1;
	return (buf_push(&exp->buf, '$'));
}

/* Metni tarar, dolar genisletmelerini cozer, gerisini harfi harfine alir. */
static int	expand_text(t_expander *exp, const char *s, int split)
{
	size_t	i;

	i = 0;
	while (s[i] != '\0')
	{
		if (s[i] == '$' && s[i + 1] != '\0')
		{
			if (expand_dollar(exp, s, &i, split) == 0)
				return (0);
			continue ;
		}
		exp->emit = 1;
		if (push_one(exp, s[i]) == 0)
			return (0);
		i++;
	}
	return (1);
}

/*
** Sozcugun basindaki ~ isaretini ev dizinine cevirir.
**
** Yalnizca tirnaksiz bir parcanin ilk karakteri oldugunda ve arkasindan
** "/" ya da parca sonu geldiginde uygulanir. "~kullanici" bicimi
** desteklenmiyor; docs/FINDINGS.md icinde kayitli.
**
** Genisletilecek metnin geri kalanini dondurur; hata halinde NULL.
*/
static const char	*expand_tilde(t_expander *exp, const char *s)
{
	const char	*home;

	if (s[0] != '~' || (s[1] != '\0' && s[1] != '/'))
		return (s);
	home = getenv("HOME");
	if (home == NULL || *home == '\0')
		return (s);
	exp->emit = 1;
	exp->literal = 1;
	if (append_plain(exp, home) == 0)
		return (NULL);
	exp->literal = 0;
	return (s + 1);
}

/*
** Tek bir parcayi kipine gore isler.
**
** TIRNAK KIPI MASKEYI DE BELIRLIYOR: tirnakli parcadan gelen her karakter
** harfi harfine, tirnaksiz parcadan gelen her karakter desen olarak
** isaretleniyor. Genisletme sonuclari da parcanin kipini devraliyor ve bu
** olculdu: tirnaksiz bir degiskenin degerindeki yildiz desen oluyor.
*/
static int	expand_seg(t_expander *exp, const t_seg *seg, int first)
{
	const char	*text;

	exp->literal = (seg->quote != Q_NONE);
	if (seg->quote == Q_SINGLE)
	{
		exp->emit = 1;
		return (append_plain(exp, seg->text));
	}
	if (seg->quote == Q_DOUBLE)
	{
		exp->emit = 1;
		return (expand_text(exp, seg->text, 0));
	}
	text = seg->text;
	if (first)
	{
		text = expand_tilde(exp, text);
		if (text == NULL)
			return (0);
	}
	return (expand_text(exp, text, 1));
}

/* Sozcuk sonunda kalan tamponu ve bos alan kuralini uygular. */
static int	finish_word(t_expander *exp)
{
	if (buf_empty(&exp->buf) == 0)
		return (field_flush(exp));
	if (exp->emit)
		return (field_add(exp, strdup(""), strdup("")));
	return (1);
}

/* Genisletici durumunu tek bir sozcuk icin kurar. */
static void	expander_init(t_expander *exp, const t_token *word,
		const t_shell *sh, t_exp_err *err)
{
	exp->sh = sh;
	exp->word = word;
	buf_init(&exp->buf);
	buf_init(&exp->mask);
	exp->literal = 0;
	exp->head = NULL;
	exp->tail = NULL;
	exp->emit = 0;
	exp->err = err;
	err->message = NULL;
	err->at = NULL;
}

/* Alan listesini serbest birakir. */
void	field_free(t_field *fields)
{
	t_field	*next;

	while (fields != NULL)
	{
		next = fields->next;
		free(fields->text);
		free(fields->mask);
		free(fields);
		fields = next;
	}
}

/*
** Ham bir metindeki degiskenleri genisletip TEK dizge dondurur.
**
** NEDEN AYRI GIRIS: "<<" govdesi bir sozcuk degil, serbest metin. Alan
** ayirma yapilmamali (bosluklar ve yenisatirlar oldugu gibi kalmak
** zorunda) ve tirnak kipi yok - govdenin tamami tek parca.
**
** Basarida 1 doner ve sonucu *out icine yazar; cagiran serbest birakir.
**
** MASKE HARFI HARFINE: "<<" govdesi dosya adi genisletmesine tabi degil,
** o yuzden uretilen her karakter harf olarak isaretleniyor. Govde zaten
** alan listesine donusmuyor; maske yalnizca tamponlarin ayni uzunlukta
** kalmasi icin dolduruluyor.
*/
int	exp_raw_text(const char *text, const t_shell *sh, char **out,
		t_exp_err *err)
{
	t_expander	exp;

	expander_init(&exp, NULL, sh, err);
	exp.literal = 1;
	if (expand_text(&exp, text, 0) == 0)
	{
		buf_free(&exp.buf);
		buf_free(&exp.mask);
		field_free(exp.head);
		if (err->message == NULL)
			err->message = "bellek ayrilamadi";
		return (0);
	}
	field_free(exp.head);
	buf_free(&exp.mask);
	if (exp.buf.data == NULL)
	{
		*out = strdup("");
		return (*out != NULL);
	}
	*out = buf_take(&exp.buf);
	return (*out != NULL);
}

/*
** Bir sozcugu genisletir ve urettigi alan listesini dondurur.
**
** Alan uretmeyen sozcuk icin de NULL doner; bu bir hata DEGILDIR. Cagiran
** ikisini err->message degerine bakarak ayirmak zorunda.
*/
t_field	*exp_word(const t_token *word, const t_shell *sh, t_exp_err *err)
{
	t_expander	exp;
	const t_seg	*seg;
	int			first;

	expander_init(&exp, word, sh, err);
	seg = word->segs;
	first = 1;
	while (seg != NULL)
	{
		if (expand_seg(&exp, seg, first) == 0)
			break ;
		first = 0;
		seg = seg->next;
	}
	if (seg == NULL && finish_word(&exp) != 0)
	{
		buf_free(&exp.buf);
		buf_free(&exp.mask);
		return (exp.head);
	}
	buf_free(&exp.buf);
	buf_free(&exp.mask);
	field_free(exp.head);
	if (err->message == NULL)
	{
		err->message = "bellek ayrilamadi";
		err->at = word;
	}
	return (NULL);
}
