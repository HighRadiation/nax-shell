/*
** heredoc.c - "<<" govdesini satir satir toplar.
**
** NEDEN AYRISTIRMADAN SONRA:
**   Kac satir okunacagi ancak sinirlayici bilindikten sonra belli olur,
**   yani sozcuk ayirma ve ayristirma bitmeden toplama baslayamaz. Bu
**   yuzden toplama ayri bir adim ve kabugun GIRDI yolunu degistiriyor:
**   okuma dongusu bir satir alirken, bu modul ayni kaynaktan birden
**   fazla satir aliyor.
**
** SIRA ONEMLI:
**   Bir satirda birden fazla "<<" olabilir ve govdeler YAZILDIKLARI
**   SIRADA okunur. "cat << A << B" satirinda once A'nin govdesi, sonra
**   B'nin govdesi gelir. Bash de boyle davraniyor.
**
** DOSYA SONU GELIRSE:
**   Sinirlayici hic gorulmeden girdi biterse toplanan kadari kullanilir
**   ve uyari basilir. Bash de boyle yapiyor; satiri tamamen reddetmek
**   kullanicinin yazdigi her seyi kaybettirirdi.
*/

#include "nax.h"
#include "parse.h"
#include "exec.h"
#include <stdlib.h>
#include <string.h>

/* Sinirlayici sozcugunun duz metnini verir; cagiran serbest birakir. */
static char	*delimiter_text(const t_token *word)
{
	t_buf		buf;
	const t_seg	*seg;

	buf_init(&buf);
	if (word == NULL)
		return (strdup(""));
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
	return (buf_take(&buf));
}

/*
** Tek bir govdeyi sinirlayiciya kadar okur; basarida 1.
**
** Satir sinirlayiciyla TAM eslesmeli. Bash de tam eslesme istiyor:
** "ENDX" satiri "END" sinirlayicisini kapatmaz.
*/
static int	collect_one(t_redir *redir, const t_shell *sh)
{
	t_buf	buf;
	char	*line;
	char	*end;
	int		ok;

	end = delimiter_text(redir->target);
	if (end == NULL)
		return (0);
	buf_init(&buf);
	ok = 1;
	while (1)
	{
		line = ln_read_more(sh);
		if (line == NULL)
		{
			ex_warn("<< govdesi dosya sonuyla kesildi");
			break ;
		}
		if (strcmp(line, end) == 0)
		{
			free(line);
			break ;
		}
		ok = buf_push_str(&buf, line) && buf_push(&buf, '\n');
		free(line);
		if (ok == 0)
			break ;
	}
	free(end);
	if (ok == 0)
		return (buf_free(&buf), 0);
	if (buf.data == NULL)
		redir->body = strdup("");
	else
		redir->body = buf_take(&buf);
	return (redir->body != NULL);
}

/*
** Listedeki tum "<<" govdelerini yazildiklari sirada toplar.
**
** Basarisizlikta 0 doner ve cagiran satiri hic calistirmaz: yarim bir
** govdeyle komut kosmak, kullanicinin yazmadigi bir girdiyi ona
** vermek olurdu.
*/
int	heredoc_fill(t_pipeline *list, const t_shell *sh)
{
	t_cmd	*cmd;
	t_redir	*redir;

	while (list != NULL)
	{
		cmd = list->cmds;
		while (cmd != NULL)
		{
			redir = cmd->redirs;
			while (redir != NULL)
			{
				if (redir->type == T_HEREDOC && collect_one(redir, sh) == 0)
					return (0);
				redir = redir->next;
			}
			cmd = cmd->next;
		}
		list = list->next;
	}
	return (1);
}
