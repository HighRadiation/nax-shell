/*
** veto.c - sekil vetolari: bas cozulse bile satir dogal dil mi.
**
** NEDEN VAR:
**   "Ilk kelime PATH'te cozuluyorsa bu bir komuttur" kurali sezgisel
**   olarak dogru gorunur ama TEK BASINA YETERSIZ. Asil tehlike Turkce
**   kelimeler degil, PATH'te gercekten var olan Ingilizce emir kipleri.
**   Bu kapta olculdu, hepsi var: find, make, test, open, who, free,
**   sort, watch, install, top, tar, head, tail, yes, date, kill.
**
**   Yani su satirlarin hepsi, o kural tek basina uygulanirsa komut
**   sanilir:
**     find all big files
**     make it faster
**     who is using port 3000
**     free up space
**
** VETO TETIKLENIRSE NE OLUR:
**   Satir sessizce niyet yoluna gider. Yardimci surecin sistem isteminde
**   "gelen metin zaten gecerli bir kabuk komutuysa aynen geri ver" kurali
**   bulunur. Boylece yanlis yonlendirmenin bedeli birkac yuz milisaniye
**   olur, YANLIS CALISTIRMA olmaz. Tasarimin her yerinde tercih bu yonde.
**
** YOL ISTISNASI:
**   ASCII disi harf ve durak kelime vetolari, o sozcuk cwd'de var olan bir
**   dosyayi adlandiriyorsa tetiklenmez. Boylece "cat sirket.txt" ve "cat
**   it" (it adli bir dosya varsa) calisir. Istisna olmasa gercek dosya
**   adlari niyet sanilirdi.
*/

#include "nax.h"
#include "ai.h"
#include "exec.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define WORDBAG_MIN 4
#define STOPWORD_MIN 2

/* Sozcugun tum parcalarini birlestirir; cagiran serbest birakir. */
char	*cls_word_text(const t_token *word)
{
	t_buf		buf;
	const t_seg	*seg;

	buf_init(&buf);
	seg = word->segs;
	while (seg != NULL)
	{
		if (buf_push_str(&buf, seg->text) == 0)
		{
			buf_free(&buf);
			return (NULL);
		}
		seg = seg->next;
	}
	if (buf.data == NULL)
		return (strdup(""));
	return (buf.data);
}

/* Metin cwd'de ya da verilen yolda var olan bir seyi adlandiriyor mu. */
static int	names_a_path(const char *text)
{
	struct stat	info;

	if (*text == '\0')
		return (0);
	return (stat(text, &info) == 0);
}

/* Metinde ASCII disi bayt var mi. */
static int	has_non_ascii(const char *text)
{
	while (*text != '\0')
	{
		if ((unsigned char)*text > 0x7f)
			return (1);
		text++;
	}
	return (0);
}

/*
** Metin bir secenek, yol ya da bicim dizesi gorunumunde mi.
**
** Karakter kumesi olculerek genisletildi: ilk yazimda yalnizca "/.*=$~"
** vardi ve "printf \"[%s]\" a b c" satiri dort ciplak kelime sanilip
** niyet yoluna gidiyordu. Koseli parantez, yuzde, suslu parantez ve
** virgul dogal dilde gecmez, bicim dizelerinde ve argumanlarda gecer.
*/
static int	looks_like_argument(const char *text)
{
	if (text[0] == '-')
		return (1);
	return (strpbrk(text, "/.*=$~[]%{},:") != NULL);
}

/*
** Metin dogal dile ozgu bir kelime mi.
**
** Liste kisa ve kasten zamir, tanimlayici ve soru kelimelerinden olusuyor:
** bunlar gercek komut argumani olarak neredeyse hic gecmez. Turkce
** kelimeler ASCII yazimiyla da listede, cunku kullanici Turkce
** karakterlerle yazdiginda ASCII disi vetosu zaten tetikleniyor.
**
** "a" ve "an" BILINCLI olarak yok: tek harfli kelimeler dosya adi ve
** arguman olarak sik gecer. "cd a b" satiri bu yuzden niyet saniliyordu.
*/
static int	is_stop_word(const char *text)
{
	static const char	*const words[] = {
		"it", "the", "this", "that", "these", "those",
		"my", "me", "all", "some", "every", "up", "down",
		"why", "how", "what", "where", "which", "who",
		"bana", "bunu", "sunu", "bu", "su", "icin", "ile",
		"goster", "listele", "bul", "kac", "hangi", "nerede",
		"neden", "nasil", "kim", "ne", NULL
	};
	size_t				i;

	i = 0;
	while (words[i] != NULL)
	{
		if (strcmp(words[i], text) == 0)
			return (1);
		i++;
	}
	return (0);
}

/*
** Satirda kabuk operatoru var mi.
**
** Boru ve yonlendirme dogal dilde gecmez, o yuzden guclu bir komut
** isaretidir. Iki yerde kullaniliyor: burada tum vetolari kapatir,
** classifier.c icinde cozulmeyen basi kabuk yoluna gonderir.
*/
int	cls_has_operator(const t_token *tokens)
{
	while (tokens != NULL)
	{
		if (tokens->type != T_WORD)
			return (1);
		tokens = tokens->next;
	}
	return (0);
}

/* Sozcuk sayisini verir. */
size_t	cls_word_count(const t_token *tokens)
{
	size_t	n;

	n = 0;
	while (tokens != NULL)
	{
		n++;
		tokens = tokens->next;
	}
	return (n);
}

/*
** V1: satir soru isaretiyle bitiyor.
**
** Soru isaretinden once bosluk olmasi ya da en az uc sozcuk bulunmasi
** sart. Aksi halde "ls foo?" gibi joker karakterli bir komut soru
** sanilirdi.
*/
static int	veto_question(const t_token *tokens, const char *line)
{
	size_t	len;

	len = strlen(line);
	while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
		len--;
	if (len == 0 || line[len - 1] != '?')
		return (0);
	if (len >= 2 && (line[len - 2] == ' ' || line[len - 2] == '\t'))
		return (VETO_QUESTION);
	if (cls_word_count(tokens) >= 3)
		return (VETO_QUESTION);
	return (0);
}

/*
** V2 ve V4: sozcuk sozcuk bakilan vetolar.
**
** DURAK KELIME BASI SAYMAZ: "who" ve "which" hem gercek komut hem soru
** kelimesi. Bas olarak gectiginde komuttur; "who -b" satiri aksi halde
** niyet sanilirdi.
**
** ASCII disi vetosu ise basi da sayar, cunku Turkce karakterli bir bas
** hicbir komuta karsilik gelmiyor.
**
** BURADA looks_like_argument CAGRILMAZ: ilk yazimda vardi, mutasyon
** testi ulasilamaz oldugunu gosterdi. Durak kelimelerin hicbirinde "-"
** oneki ya da yol karakteri yok, yani is_stop_word dogruyken
** looks_like_argument her zaman yanlis donuyordu.
*/
static int	veto_words(const t_token *tokens)
{
	char	*text;
	int		found;
	size_t	n;
	int		is_head;

	found = 0;
	n = cls_word_count(tokens);
	is_head = 1;
	while (tokens != NULL)
	{
		text = cls_word_text(tokens);
		if (text == NULL)
			return (found);
		if (names_a_path(text) == 0)
		{
			if (has_non_ascii(text))
				found |= VETO_NONASCII;
			if (is_head == 0 && n >= STOPWORD_MIN && is_stop_word(text))
				found |= VETO_STOPWORD;
		}
		free(text);
		is_head = 0;
		tokens = tokens->next;
	}
	return (found);
}

/*
** V3: ciplak kelime torbasi.
**
** Dort ya da daha fazla sozcuk, hicbiri secenek ya da yol gorunumunde
** degil ve hicbiri var olan bir seyi adlandirmiyor. "who is using port
** 3000" ve "sort these by date" boyle yakalaniyor.
*/
static int	veto_wordbag(const t_token *tokens)
{
	char	*text;
	int		plain;

	if (cls_word_count(tokens) < WORDBAG_MIN)
		return (0);
	while (tokens != NULL)
	{
		text = cls_word_text(tokens);
		if (text == NULL)
			return (0);
		plain = (looks_like_argument(text) == 0 && names_a_path(text) == 0);
		free(text);
		if (plain == 0)
			return (0);
		tokens = tokens->next;
	}
	return (VETO_WORDBAG);
}

/* git alt komutu tanidik mi. */
static int	is_git_subcommand(const char *text)
{
	static const char	*const subs[] = {
		"add", "am", "apply", "archive", "bisect", "blame", "branch",
		"bundle", "checkout", "cherry-pick", "clean", "clone", "commit",
		"config", "describe", "diff", "fetch", "gc", "grep", "init",
		"log", "merge", "mv", "notes", "pull", "push", "rebase",
		"reflog", "remote", "reset", "restore", "revert", "rm",
		"shortlog", "show", "stash", "status", "submodule", "switch",
		"tag", "worktree", NULL
	};
	size_t				i;

	i = 0;
	while (subs[i] != NULL)
	{
		if (strcmp(subs[i], text) == 0)
			return (1);
		i++;
	}
	return (0);
}

/* Basa ozel dogrulayici: ikinci sozcuk bu bas icin makul mu. */
static int	head_arg_is_sane(const char *head, const char *arg)
{
	if (arg == NULL)
		return (1);
	if (arg[0] == '-')
		return (1);
	if (strcmp(head, "git") == 0)
		return (is_git_subcommand(arg));
	if (strcmp(head, "find") == 0)
		return (names_a_path(arg));
	if (strcmp(head, "test") == 0)
		return (names_a_path(arg));
	if (strcmp(head, "make") == 0)
		return (names_a_path("Makefile") || names_a_path("makefile"));
	return (1);
}

/*
** V5: belirsiz bas.
**
** Yalnizca kendi dogrulayicisi OLAN baslar icin bakilir: git, find, test,
** make. Digerleri V3 ve V4'e birakildi, cunku her bas icin dogrulayici
** yazmak hem bitmez hem de her biri yeni bir yanlis pozitif kaynagi olur.
*/
static int	veto_head(const t_token *tokens)
{
	char	*head;
	char	*arg;
	int		found;

	head = cls_word_text(tokens);
	if (head == NULL)
		return (0);
	arg = NULL;
	if (tokens->next != NULL && tokens->next->type == T_WORD)
		arg = cls_word_text(tokens->next);
	found = 0;
	if (head_arg_is_sane(head, arg) == 0)
		found = VETO_HEAD;
	free(head);
	free(arg);
	return (found);
}

/*
** Tetiklenen vetolarin maskesini dondurur; sifir ise satir komuttur.
**
** Satirda kabuk operatoru varsa hicbir veto bakilmaz: boru ve
** yonlendirme dogal dilde gecmez, o yuzden guclu bir komut isaretidir.
*/
int	cls_vetoes(const t_token *tokens, const char *line)
{
	int	found;

	if (tokens == NULL || tokens->type != T_WORD)
		return (0);
	if (cls_has_operator(tokens))
		return (0);
	found = veto_question(tokens, line);
	found |= veto_words(tokens);
	found |= veto_wordbag(tokens);
	found |= veto_head(tokens);
	return (found);
}
