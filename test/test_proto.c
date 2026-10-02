/*
** test_proto.c - tel biciminin kurulmasi ve cozulmesi.
**
** UC VAKA TURU, GIRDININ ONEKINDEN ANLASILIYOR:
**
**   P:<satir>        Satir cozulur. Beklenen ya kanonik dokum ya da
**                    "ERR:<ad>" bicimde hata adi.
**   B:TIP|id=N|k=v   Kayit KURULUR ve beklenen tam tel satiri. Sondaki
**                    yenisatir karsilastirmaya girmez, ayrica ayri bir
**                    kural olarak dogrulanir.
**   C:<ad>           Tabloda anlatilamayan kontrol; govdesi C icinde.
**
** KANONIK DOKUM: TIP|anahtar=deger|anahtar=deger
**
** Denetim karakterleri dokumde <TAB>, <NL> ve <%02x> olarak gosterilir.
** NEDEN KACIS DIZISI DEGIL: iskelenin kacis cozucusu "\t" taniyor ama
** "\n" tanimiyor ve tanitmak lexer korpusunun anlamini degistirirdi -
** orada "\n" gercek bir ters bolu ile n demek. Gorunur etiketler bu
** catismayi tamamen ortadan kaldiriyor.
*/

#include "nax.h"
#include "proto.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Degeri denetim karakterlerini gorunur yaparak ekler. */
static void	append_value(char *out, size_t cap, const char *value)
{
	char	piece[8];
	size_t	i;

	i = 0;
	while (value[i] != '\0')
	{
		if (value[i] == '\t')
			test_append(out, cap, "<TAB>");
		else if (value[i] == '\n')
			test_append(out, cap, "<NL>");
		else if ((unsigned char)value[i] < 0x20)
		{
			snprintf(piece, sizeof(piece), "<%02x>",
				(unsigned char)value[i]);
			test_append(out, cap, piece);
		}
		else
		{
			piece[0] = value[i];
			piece[1] = '\0';
			test_append(out, cap, piece);
		}
		i++;
	}
}

/* Cerceveyi kanonik dokume cevirir. */
static void	dump_frame(const t_frame *frame, char *out, size_t cap)
{
	size_t	i;

	out[0] = '\0';
	test_append(out, cap, proto_type_name(frame->type));
	i = 0;
	while (i < frame->count)
	{
		test_append(out, cap, "|");
		test_append(out, cap, frame->fields[i].key);
		test_append(out, cap, "=");
		append_value(out, cap, frame->fields[i].value);
		i++;
	}
}

/* Cozme vakasi. */
static void	case_parse(t_score *score, int no, char *input, char *want)
{
	t_frame	frame;
	t_perr	err;
	char	got[4096];

	err = proto_parse(input, &frame);
	if (err != PE_OK)
		snprintf(got, sizeof(got), "ERR:%s", proto_err_name(err));
	else
		dump_frame(&frame, got, sizeof(got));
	if (strcmp(got, want) == 0)
		score->passed++;
	else
		report_fail(score, no, input, want, got);
	proto_free(&frame);
}

/* Kurma belirtimini tipe, kimlige ve alanlara ayirir; alan sayisini verir. */
static size_t	split_spec(char *input, t_ftype *type, long *id,
		t_pair *fields)
{
	char	*token;
	char	*eq;
	size_t	n;

	*type = FR_UNKNOWN;
	*id = 0;
	token = strtok(input, "|");
	if (token != NULL)
		*type = proto_type_from(token);
	token = strtok(NULL, "|");
	if (token != NULL && strncmp(token, "id=", 3) == 0)
		*id = atol(token + 3);
	n = 0;
	token = strtok(NULL, "|");
	while (token != NULL && n < PROTO_MAX_FIELDS)
	{
		eq = strchr(token, '=');
		if (eq == NULL)
			return (n);
		*eq = '\0';
		fields[n].key = token;
		fields[n].value = eq + 1;
		n++;
		token = strtok(NULL, "|");
	}
	return (n);
}

/*
** Kurulan satirin yenisatir kurali: tam olarak sonda ve tam olarak bir
** tane. Her kurma vakasinda dogrulanir, cunku eksik ya da fazla
** yenisatir akista cerceve sinirini kaydirir ve bunu tabloda her satira
** yazmak gurultu olurdu.
*/
static int	newline_is_sane(const char *line)
{
	size_t	len;

	len = strlen(line);
	if (len == 0 || line[len - 1] != '\n')
		return (0);
	return (strchr(line, '\n') == line + len - 1);
}

/* Kurma vakasi. */
static void	case_build(t_score *score, int no, char *input, char *want)
{
	t_pair	fields[PROTO_MAX_FIELDS];
	t_ftype	type;
	long	id;
	size_t	n;
	char	*line;

	n = split_spec(input, &type, &id, fields);
	line = proto_build(type, id, fields, n);
	if (line == NULL)
	{
		if (strcmp(want, "NULL") == 0)
			score->passed++;
		else
			report_fail(score, no, input, want, "NULL");
		return ;
	}
	if (newline_is_sane(line) == 0)
		report_fail(score, no, input, want, "yenisatir kurali bozuk");
	else if (strncmp(line, want, strlen(want)) == 0
		&& strlen(line) == strlen(want) + 1)
		score->passed++;
	else
		report_fail(score, no, input, want, line);
	free(line);
}

/* Satir uzunlugu sinirini olcer: sinirin biri ustu reddedilmeli. */
static int	check_too_long(void)
{
	t_frame	frame;
	char	*line;
	t_perr	over;
	t_perr	exact;

	line = malloc(PROTO_MAX_LINE + 2);
	if (line == NULL)
		return (0);
	memset(line, 'x', PROTO_MAX_LINE + 1);
	line[PROTO_MAX_LINE + 1] = '\0';
	over = proto_parse(line, &frame);
	proto_free(&frame);
	line[PROTO_MAX_LINE] = '\0';
	exact = proto_parse(line, &frame);
	proto_free(&frame);
	free(line);
	return (over == PE_TOO_LONG && exact != PE_TOO_LONG);
}

/* Yenisatir iceren deger kodlanip aynen geri cozulmeli. */
static int	check_newline_round_trip(void)
{
	t_pair	field;
	t_frame	frame;
	char	*line;
	int		ok;

	field.key = "text";
	field.value = "bir\niki\tuc";
	line = proto_build(FR_OK, 3, &field, 1);
	if (line == NULL)
		return (0);
	ok = (strstr(line, "text=b64:") != NULL);
	ok = ok && (proto_parse(line, &frame) == PE_OK);
	ok = ok && (proto_field(&frame, "text") != NULL)
		&& strcmp(proto_field(&frame, "text"), "bir\niki\tuc") == 0;
	proto_free(&frame);
	free(line);
	return (ok);
}

/* Serbest birakma iki kez cagrilabilmeli; olmayan alan NULL vermeli. */
static int	check_free_and_missing(void)
{
	t_frame	frame;
	int		ok;

	ok = (proto_parse("READY\tid=0\tversion=1", &frame) == PE_OK);
	ok = ok && (proto_field(&frame, "key") == NULL);
	ok = ok && (proto_field(&frame, "version") != NULL);
	proto_free(&frame);
	proto_free(&frame);
	return (ok);
}

/*
** Sondaki yenisatir yok sayilmali.
**
** Tabloda anlatilamiyor: iskelenin kacis cozucusu "\n" tanimiyor, o yuzden
** vaka dosyasina gercek yenisatir konulamaz. Kural yine de olculmek
** zorunda, cunku akistan gelen her satir yenisatirla bitiyor.
*/
static int	check_trailing_newline(void)
{
	t_frame	frame;
	int		ok;

	ok = (proto_parse("BYE\n", &frame) == PE_OK);
	ok = ok && frame.type == FR_BYE && frame.count == 0;
	proto_free(&frame);
	ok = ok && (proto_parse("READY\tid=0\tversion=1\n", &frame) == PE_OK);
	ok = ok && (proto_field(&frame, "version") != NULL)
		&& strcmp(proto_field(&frame, "version"), "1") == 0;
	proto_free(&frame);
	return (ok);
}

/* Adi verilen kontrolu kosar. */
static void	case_check(t_score *score, int no, char *input, char *want)
{
	int	ok;

	if (strcmp(input, "too_long") == 0)
		ok = check_too_long();
	else if (strcmp(input, "newline_round_trip") == 0)
		ok = check_newline_round_trip();
	else if (strcmp(input, "free_and_missing") == 0)
		ok = check_free_and_missing();
	else if (strcmp(input, "trailing_newline") == 0)
		ok = check_trailing_newline();
	else
	{
		report_fail(score, no, input, want, "boyle bir kontrol yok");
		return ;
	}
	if (ok)
		score->passed++;
	else
		report_fail(score, no, input, want, "kontrol basarisiz");
}

/* Vakayi onekine gore dogru isleve yonlendirir. */
static void	run_case(t_score *score, int no, char *input, char *want)
{
	if (strncmp(input, "P:", 2) == 0)
		case_parse(score, no, input + 2, want);
	else if (strncmp(input, "B:", 2) == 0)
		case_build(score, no, input + 2, want);
	else if (strncmp(input, "C:", 2) == 0)
		case_check(score, no, input + 2, want);
	else
		report_fail(score, no, input, want, "onek yok (P: B: C: bekleniyor)");
}

/* Giris noktasi; vaka dosyasi yolu argumanla degistirilebilir. */
int	main(int argc, char **argv)
{
	const char	*given;

	given = "test/cases/proto.tsv";
	if (argc > 1)
		given = argv[1];
	return (harness_run(given, "proto testleri", run_case));
}
