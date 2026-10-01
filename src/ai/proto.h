/*
** proto.h - kabuk ile yardimci surec arasindaki tel biciminin tipleri.
**
** NEDEN AYRI BASLIK:
**   Tel bicimi AI tarafinin ici degil, iki surec arasindaki SOZLESME.
**   ai.h satirin komut mu niyet mi oldugunu karara baglar; bu baslik ise
**   o kararin karsi tarafa nasil tasindigini anlatir. Ikisini ayri
**   tutmak, protokol degistiginde nerenin etkilendigini gorunur kiliyor.
**
** BICIM:
**   <TIP>\tid=<n>\tanahtar=deger\tanahtar=b64:...\n
**
**   Bir satir bir kayit. Serbest metin ve sekme/yenisatir icerebilen
**   degerler "b64:" onekiyle kodlanir, geri kalan alanlar okunabilir
**   kalir - gunlukte "id=7 kind=request" gormek hata ayiklarken
**   kazanilan baytlardan degerli.
**
**   Gerekcesinin tamami docs/PROTOCOL.md icinde.
**
** SAHIPLIK:
**   Cozulen cerceve TEK bir tampona sahiptir ve tum anahtar/deger
**   isaretcileri o tamponun icine bakar. Boylece alan sayisi kadar
**   ayirma yapilmiyor ve proto_free tek cagriyla her seyi birakiyor.
**
**   base64 cozme YERINDE yapilabiliyor, cunku cozulen veri her zaman
**   kodlanmis halinden kisa. Bu, ayri bir tampon ve ikinci bir omur
**   sorusu olmamasi demek.
*/

#ifndef PROTO_H
# define PROTO_H

# include <stddef.h>
# include "nax.h"

/*
** Kayit tipleri.
**
** Ilk grup kabuktan yardimci surece, ikinci grup ters yone gider.
** FR_UNKNOWN taninmayan tip; cozme bunu hata sayar ama tipin kendisi
** gerekli, cunku ayirma ile dogrulama ayri adimlar.
*/
typedef enum e_ftype
{
	FR_UNKNOWN,
	FR_HELLO,
	FR_INTENT,
	FR_EXPLAIN,
	FR_EVENT,
	FR_CANCEL,
	FR_BYE,
	FR_READY,
	FR_OK,
	FR_NEED,
	FR_ERR
}	t_ftype;

/*
** Cozme hatalari.
**
** Her biri ayri, cunku dayaniklilik kurallari bunlara gore farkli
** davraniyor: PE_TOO_LONG protokol ihlali sayilip yardimci sureci
** yeniden baslatirken, PE_BAD_TYPE yalnizca o satirin atilmasina yol
** acar. Tek bir "gecersiz" degeri bu ayrimi imkansiz kilardi.
*/
typedef enum e_perr
{
	PE_OK,
	PE_EMPTY,
	PE_TOO_LONG,
	PE_BAD_TYPE,
	PE_BAD_FIELD,
	PE_TOO_MANY,
	PE_BAD_B64,
	PE_NUL,
	PE_NOMEM
}	t_perr;

# define PROTO_MAX_LINE 1048576
# define PROTO_MAX_FIELDS 16

/* Bir anahtar/deger cifti; ikisi de cercevenin tamponuna bakar. */
typedef struct s_field
{
	const char	*key;
	const char	*value;
}	t_field;

/* Cozulmus bir kayit; buf sahipli, alanlar onun icine isaret eder. */
typedef struct s_frame
{
	t_ftype	type;
	char	*buf;
	t_field	fields[PROTO_MAX_FIELDS];
	size_t	count;
}	t_frame;

/*
** Okuma tamponunun dort durumu.
**
** Bicimin dayaniklilik kurallari bunlarin AYRI olmasini gerektiriyor:
**
**   RD_LINE      tam bir satir hazir
**   RD_MORE      satir yarim; daha veri gerekiyor
**   RD_EOF       akis bitti. Elde yarim satir kaldiysa ATILIR, cunku
**                yarim bir kayit cozulemez ve tamamlanma sansi yok
**   RD_TOO_LONG  satir sinirini asti; protokol ihlali
**   RD_ERROR     okuma hatasi
**
** "Yarim satir" ile "akis bitti"yi tek degerde birlestirmek, bekleyen
** isteginin iptal edilmesi gerekip gerekmedigini belirsiz kilardi.
*/
typedef enum e_readst
{
	RD_LINE,
	RD_MORE,
	RD_EOF,
	RD_TOO_LONG,
	RD_ERROR
}	t_readst;

/*
** Satir tabanli okuma tamponu.
**
** pending ONEMLI: rd_take dondurdugu satiri tamponun ICINDE gosterir ve
** o satiri hemen atmaz, cunku atmak memmove ile veriyi kaydirmak ve
** cagiranin elindeki isaretciyi gecersiz kilmak olurdu. Atma islemi bir
** SONRAKI rd_take cagrisinin basinda yapiliyor.
**
** drop, asiri uzun satirin kalanini atlama kipi: sinir asildiginda
** tampon bosaltilir ve bir sonraki yenisatira kadar gelen her sey
** atilir. Satir tabanli olmanin asil kazanci bu kurtarma yetenegi.
*/
typedef struct s_reader
{
	int		fd;
	char	*data;
	size_t	len;
	size_t	cap;
	size_t	pending;
	int		drop;
	int		ended;
}	t_reader;

char		*b64_encode(const unsigned char *in, size_t n);
int			b64_decode_inplace(char *text, size_t *out_len);

char		*proto_build(t_ftype type, long id, const t_field *fields,
				size_t n);
t_perr		proto_parse(const char *line, t_frame *out);
void		proto_free(t_frame *frame);
const char	*proto_field(const t_frame *frame, const char *key);
const char	*proto_type_name(t_ftype type);
t_ftype		proto_type_from(const char *name);
const char	*proto_err_name(t_perr err);

void		rd_init(t_reader *reader, int fd);
void		rd_free(t_reader *reader);
t_readst	rd_feed(t_reader *reader);
t_readst	rd_take(t_reader *reader, char **line);
const char	*rd_state_name(t_readst state);

#endif
