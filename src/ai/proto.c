/*
** proto.c - tel bicimini kurma ve cozme.
**
** HANGI ALAN OKUNABILIR KALIR:
**   Deger yalnizca "sade" ise oldugu gibi gonderilir: bos degil, harf,
**   rakam ve "_.:/-," disinda karakter icermiyor ve "b64:" ile
**   baslamiyor. Boylece id, kind, status, branch ve danger gunlukte
**   okunabilir kaliyor; kullanicinin yazdigi her sey kodlaniyor.
**
**   "b64:" ile baslayan degerin kodlanmasi SART: aksi halde cozen taraf
**   onu kodlanmis sanip bozuk veri uretirdi. Bu, bicimin kendi kacis
**   sorunu ve tek satirla kapaniyor.
**
** BOS DEGER:
**   Sade sayilmaz, yani "b64:" olarak gider ve karsida bos dizgiye
**   cozulur. Boylece "deger yok" ile "deger bos" ayrimi korunuyor.
**
** COZMEDE NEDEN TEK TAMPON:
**   Satirin bir kopyasi alinir, sekmeler NUL'a cevrilir ve tum
**   isaretciler o kopyanin icine bakar. base64 cozmesi de yerinde
**   yapiliyor. Sonuc: alan sayisi ne olursa olsun bir ayirma, bir
**   serbest birakma.
*/

#include "proto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Tip adlarini sirayla verir; dizi t_ftype ile ayni duzende. */
static const char	*const	*type_names(void)
{
	static const char	*const	names[] = {
		"UNKNOWN", "HELLO", "INTENT", "EXPLAIN", "EVENT", "CANCEL",
		"BYE", "READY", "OK", "NEED", "ERR", NULL
	};

	return (names);
}

/* Tipin tel uzerindeki adini verir. */
const char	*proto_type_name(t_ftype type)
{
	const char	*const	*names;
	size_t				i;

	names = type_names();
	i = 0;
	while (names[i] != NULL)
	{
		if (i == (size_t)type)
			return (names[i]);
		i++;
	}
	return (names[0]);
}

/*
** Addan tipe cevirir; taninmayan ad FR_UNKNOWN verir.
**
** Arama birden basliyor cunku "UNKNOWN" tel uzerinde bir ad degil,
** tanimama durumunun kendisi. Sifirdan baslamak da ayni sonucu verirdi -
** eslesme FR_UNKNOWN donerdi ve cagiran onu zaten hata sayiyor - yani bu
** secim davranisi degil niyeti anlatiyor.
*/
t_ftype	proto_type_from(const char *name)
{
	const char	*const	*names;
	size_t				i;

	names = type_names();
	i = 1;
	while (names[i] != NULL)
	{
		if (strcmp(names[i], name) == 0)
			return ((t_ftype)i);
		i++;
	}
	return (FR_UNKNOWN);
}

/* Hata adini verir; testler ve gunluk icin. */
const char	*proto_err_name(t_perr err)
{
	static const char	*const	names[] = {
		"OK", "EMPTY", "TOO_LONG", "BAD_TYPE", "BAD_FIELD", "TOO_MANY",
		"BAD_B64", "NUL", "NOMEM", NULL
	};
	size_t				i;

	i = 0;
	while (names[i] != NULL)
	{
		if (i == (size_t)err)
			return (names[i]);
		i++;
	}
	return ("OK");
}

/* Anahtar gecerli mi; harf, rakam ve alt cizgi. */
static int	key_is_valid(const char *key)
{
	size_t	i;

	if (key == NULL || *key == '\0')
		return (0);
	i = 0;
	while (key[i] != '\0')
	{
		if (isalnum((unsigned char)key[i]) == 0 && key[i] != '_')
			return (0);
		i++;
	}
	return (1);
}

/* Deger oldugu gibi gonderilebilir mi. */
static int	value_is_plain(const char *value)
{
	size_t	i;

	if (value == NULL || *value == '\0')
		return (0);
	if (strncmp(value, "b64:", 4) == 0)
		return (0);
	i = 0;
	while (value[i] != '\0')
	{
		if (isalnum((unsigned char)value[i]) == 0
			&& strchr("_.:/-,", value[i]) == NULL)
			return (0);
		i++;
	}
	return (1);
}

/* Degeri sade ya da kodlanmis halde tampona yazar. */
static int	push_value(t_buf *buf, const char *value)
{
	char	*encoded;
	int		ok;

	if (value == NULL)
		value = "";
	if (value_is_plain(value))
		return (buf_push_str(buf, value));
	encoded = b64_encode((const unsigned char *)value, strlen(value));
	if (encoded == NULL)
		return (0);
	ok = buf_push_str(buf, "b64:") && buf_push_str(buf, encoded);
	free(encoded);
	return (ok);
}

/* Bir alani sekmeyle birlikte tampona yazar. */
static int	push_field(t_buf *buf, const t_field *field)
{
	if (key_is_valid(field->key) == 0)
		return (0);
	if (buf_push(buf, '\t') == 0 || buf_push_str(buf, field->key) == 0)
		return (0);
	if (buf_push(buf, '=') == 0)
		return (0);
	return (push_value(buf, field->value));
}

/*
** Bir kayit satiri kurar; cagiran serbest birakir, sonda yenisatir var.
**
** Gecersiz tip, fazla alan ya da gecersiz anahtar halinde NULL doner.
** Bunlar cagiranin hatasi, karsi tarafin degil, o yuzden ayri hata
** degerleri yok.
*/
char	*proto_build(t_ftype type, long id, const t_field *fields, size_t n)
{
	t_buf	buf;
	char	head[64];
	size_t	i;
	int		ok;

	if (type == FR_UNKNOWN || n > PROTO_MAX_FIELDS)
		return (NULL);
	buf_init(&buf);
	snprintf(head, sizeof(head), "%s\tid=%ld", proto_type_name(type), id);
	ok = buf_push_str(&buf, head);
	i = 0;
	while (ok && i < n)
	{
		ok = push_field(&buf, &fields[i]);
		i++;
	}
	ok = ok && buf_push(&buf, '\n');
	if (ok == 0)
	{
		buf_free(&buf);
		return (NULL);
	}
	return (buf_take(&buf));
}

/*
** Tek bir "anahtar=deger" parcasini cerceveye ekler.
**
** Metin cercevenin kendi tamponunda ve YERINDE degistiriliyor: esittir
** isareti NUL'a cevrilir, kodlanmis deger yerinde cozulur.
*/
static t_perr	add_field(t_frame *out, char *text)
{
	char	*eq;
	size_t	len;

	if (out->count >= PROTO_MAX_FIELDS)
		return (PE_TOO_MANY);
	eq = strchr(text, '=');
	if (eq == NULL)
		return (PE_BAD_FIELD);
	*eq = '\0';
	eq++;
	if (key_is_valid(text) == 0)
		return (PE_BAD_FIELD);
	if (strncmp(eq, "b64:", 4) == 0)
	{
		eq += 4;
		if (b64_decode_inplace(eq, &len) == 0)
			return (PE_BAD_B64);
		if (strlen(eq) != len)
			return (PE_NUL);
	}
	out->fields[out->count].key = text;
	out->fields[out->count].value = eq;
	out->count++;
	return (PE_OK);
}

/* Tamponu sekmelerden bolup tip ve alanlari yerlestirir. */
static t_perr	split_frame(t_frame *out)
{
	char	*part;
	char	*tab;
	t_perr	err;

	part = out->buf;
	tab = strchr(part, '\t');
	if (tab != NULL)
		*tab = '\0';
	out->type = proto_type_from(part);
	if (out->type == FR_UNKNOWN)
		return (PE_BAD_TYPE);
	while (tab != NULL)
	{
		part = tab + 1;
		tab = strchr(part, '\t');
		if (tab != NULL)
			*tab = '\0';
		err = add_field(out, part);
		if (err != PE_OK)
			return (err);
	}
	return (PE_OK);
}

/*
** Bir kayit satirini cozer; PE_OK disinda bir sey donerse cerceve
** kullanilmaz ama proto_free yine cagrilmalidir.
**
** Sondaki yenisatir varsa yok sayilir: ayni cozucu hem akistan gelen
** satirlari hem testteki ciplak dizgileri kabul edebilsin diye.
*/
t_perr	proto_parse(const char *line, t_frame *out)
{
	size_t	len;

	proto_blank(out);
	if (line == NULL)
		return (PE_EMPTY);
	len = strlen(line);
	if (len > 0 && line[len - 1] == '\n')
		len--;
	if (len > PROTO_MAX_LINE)
		return (PE_TOO_LONG);
	if (len == 0)
		return (PE_EMPTY);
	out->buf = malloc(len + 1);
	if (out->buf == NULL)
		return (PE_NOMEM);
	memcpy(out->buf, line, len);
	out->buf[len] = '\0';
	return (split_frame(out));
}

/*
** Cerceveyi BOS duruma getirir; hicbir sey serbest BIRAKMAZ.
**
** NEDEN AYRI ISLEV: cozme ve bekleme islevleri kendilerine verilen
** cerceveyi bastan kuruyor. Kurulmamis bir cerceve ile cagrildiklarinda
** proto_free cagirmak cop isaretciyi serbest birakmak olurdu. Sozlesme
** su: bu islevler cerceveyi KURAR, eski icerigi cagiran birakmis olmak
** zorunda.
*/
void	proto_blank(t_frame *frame)
{
	frame->type = FR_UNKNOWN;
	frame->buf = NULL;
	frame->count = 0;
}

/* Cercevenin tamponunu birakir; iki kez cagrilmasi guvenli. */
void	proto_free(t_frame *frame)
{
	free(frame->buf);
	frame->buf = NULL;
	frame->count = 0;
	frame->type = FR_UNKNOWN;
}

/* Anahtara karsilik gelen degeri verir; yoksa NULL. */
const char	*proto_field(const t_frame *frame, const char *key)
{
	size_t	i;

	i = 0;
	while (i < frame->count)
	{
		if (strcmp(frame->fields[i].key, key) == 0)
			return (frame->fields[i].value);
		i++;
	}
	return (NULL);
}
