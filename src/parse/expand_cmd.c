/*
** expand_cmd.c - komut duzeyi genisletme.
**
** NEDEN AYRI MODUL:
**   expand.c bir SOZCUGU alanlara cevirir; burasi bir KOMUTUN tum
**   argumanlarini ve yonlendirmelerini o isleve vererek calistiricinin
**   kullanabilecegi hale getirir. Iki farkli is, ve bu modul expand.c'nin
**   yalnizca genel arayuzunu kullaniyor, hicbir ic fonksiyonuna
**   dokunmuyor; dikis yeri tam burasi.
**
** BORU HATTI DONGUSU BURADA YOK:
**   Boru hattinin tamami icin paralel bir agac KURULMAZ. Calistirici her
**   komutu calistirmadan hemen once genisletir, kullanir ve birakir; bu
**   yuzden t_xcmd'de next alani yok.
*/

#include "nax.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>

/* Genisletilmis yonlendirme listesini serbest birakir. */
static void	xredir_free(t_xredir *redir)
{
	t_xredir	*next;

	while (redir != NULL)
	{
		next = redir->next;
		free(redir->path);
		free(redir);
		redir = next;
	}
}

/* Genisletilmis komutun sahip oldugu her seyi birakir ve sifirlar. */
void	xcmd_free(t_xcmd *xcmd)
{
	field_free(xcmd->args);
	xredir_free(xcmd->redirs);
	xcmd->args = NULL;
	xcmd->redirs = NULL;
}

/*
** Yonlendirme hedefini tek bir yola cozer.
**
** DOSYA ADI GENISLETMESI BURADA DA UYGULANIYOR: bash da yonlendirme
** hedefini genisletiyor. Birden fazla eslesme "belirsiz yonlendirme"
** demek - hangi dosyaya yazilacagi belirsizken bir tanesini secmek
** sessizce yanlis dosyaya yazmak olurdu.
*/
static int	resolve_target(const t_token *word, const t_shell *sh,
		char **out, t_exp_err *err)
{
	t_field	*fields;

	fields = exp_word(word, sh, err);
	if (fields != NULL)
		fields = glob_fields(fields);
	if (fields == NULL)
	{
		if (err->message == NULL)
		{
			err->message = "belirsiz yonlendirme";
			err->at = word;
		}
		return (0);
	}
	if (fields->next != NULL)
	{
		field_free(fields);
		err->message = "belirsiz yonlendirme";
		err->at = word;
		return (0);
	}
	*out = fields->text;
	free(fields->mask);
	free(fields);
	return (1);
}

/*
** "<<" govdesini genisletir ya da oldugu gibi kopyalar.
**
** Sinirlayici tirnakliysa govde HARFI HARFINE gider; tirnaksizsa icindeki
** degiskenler genisletilir. Kural sinirlayicinin yazimina bakiyor,
** govdenin icerigine degil - bash de boyle davraniyor ve bu, kullanicinin
** "$" iceren bir metni aynen gondermesinin tek yolu.
**
** ALAN AYIRMA YOK: govde tek bir metin, arguman listesi degil. Bosluklar
** ve yenisatirlar oldugu gibi kalmak zorunda.
*/
int	exp_heredoc(const t_redir *redir, const t_shell *sh, char **out)
{
	t_exp_err	err;

	if (redir->body == NULL)
		return (0);
	if (redir->raw)
	{
		*out = strdup(redir->body);
		return (*out != NULL);
	}
	return (exp_raw_text(redir->body, sh, out, &err));
}

/* Cozulmus yonlendirmeyi listenin sonuna ekler. */
static int	xredir_push(t_xcmd *out, t_xredir **tail, t_tok type, char *path)
{
	t_xredir	*redir;

	redir = malloc(sizeof(*redir));
	if (redir == NULL)
	{
		free(path);
		return (0);
	}
	redir->type = type;
	redir->path = path;
	redir->next = NULL;
	if (*tail == NULL)
		out->redirs = redir;
	else
		(*tail)->next = redir;
	*tail = redir;
	return (1);
}

/* Komutun tum argumanlarini genisletip tek bir alan listesine dizer. */
static int	expand_args(const t_cmd *cmd, const t_shell *sh, t_xcmd *out,
		t_exp_err *err)
{
	const t_arg	*arg;
	t_field		*tail;
	t_field		*fields;

	tail = NULL;
	arg = cmd->args;
	while (arg != NULL)
	{
		fields = exp_word(arg->word, sh, err);
		if (fields == NULL && err->message != NULL)
			return (0);
		if (fields != NULL)
			fields = glob_fields(fields);
		if (fields != NULL && tail == NULL)
			out->args = fields;
		else if (fields != NULL)
			tail->next = fields;
		while (fields != NULL)
		{
			tail = fields;
			fields = fields->next;
		}
		arg = arg->next;
	}
	return (1);
}

/*
** Komutun tum yonlendirmelerini sirasi korunarak cozer.
**
** "<<" AYRI YOLDAN GECER: onun hedefi bir dosya yolu degil sinirlayici ve
** cozulecek sey toplanan govde. Ayni dongude ele alinmasi, sirasinin
** digerlerine gore korunmasini sagliyor.
*/
static int	expand_redirs(const t_cmd *cmd, const t_shell *sh, t_xcmd *out,
		t_exp_err *err)
{
	const t_redir	*redir;
	t_xredir		*tail;
	char			*path;

	tail = NULL;
	redir = cmd->redirs;
	while (redir != NULL)
	{
		if (redir->type == T_HEREDOC)
		{
			if (exp_heredoc(redir, sh, &path) == 0)
				return (0);
		}
		else if (resolve_target(redir->target, sh, &path, err) == 0)
			return (0);
		if (xredir_push(out, &tail, redir->type, path) == 0)
			return (0);
		redir = redir->next;
	}
	return (1);
}

/* Bir komutu genisletir; basarisizlikta out bosaltilir ve 0 doner. */
int	exp_cmd(const t_cmd *cmd, const t_shell *sh, t_xcmd *out, t_exp_err *err)
{
	out->args = NULL;
	out->redirs = NULL;
	err->message = NULL;
	err->at = NULL;
	if (expand_args(cmd, sh, out, err) == 0 || expand_redirs(cmd, sh, out,
			err) == 0)
	{
		xcmd_free(out);
		if (err->message == NULL)
			err->message = "bellek ayrilamadi";
		return (0);
	}
	return (1);
}
