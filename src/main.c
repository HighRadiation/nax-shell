/*
** SPDX-License-Identifier: GPL-3.0-or-later
**
** nax - AI'i kabugun dilinin icine koyan bir komut kabugu.
** Copyright (C) 2026 Bugra Oksuz
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <https://www.gnu.org/licenses/>.
** Bu depoda LICENSE dosyasinda duruyor.
**
** NEDEN BILDIRIM INGILIZCE: lisans metnini ceviren bir bildirim, lisans
** tarayicilari (SPDX, scancode) tarafindan TANINMIYOR. Bildirim hukuki
** bir beyan, kodu anlatan bir yorum degil; o yuzden burada projenin
** yorum dili kurali disinda kaliyor.
*/

/*
** main.c - giris noktasi ve okuma dongusu.
**
** NEDEN VAR:
**   Kabugun omru burada baslar ve biter: durumu kur, satirlari oku, isle,
**   sonra her seyi geri birak. Satirin nasil okundugu line.c'nin isi;
**   satirin ne anlama geldigi ise siniflandiriciya (classifier.c) gececek.
**
** SATIR NASIL ISLENIYOR:
**   Satir once SINIFLANDIRILIYOR: kabuk komutu mu, dogal dil mi. Kabuk
**   komutuysa ayristirilip calistiriliyor; dogal dil bes yoldan birine
**   gidiyor (yazim duzeltmesi, sozdizimi hatasi, niyet, aciklama, bos).
**
**   Bos satir ve denetim karakteri filtresi de siniflandiricinin isi;
**   burada ayri bir is_blank kontrolu YOK, cunku iki yerde duran bir
**   filtre tek bir yerde duzeltildiginde sessizce ayrisir.
**
**   "exit" ozel bir durum DEGIL, gercek bir yerlesik. Satiri daha
**   ayristirmadan yakalayan bir kestirme kullanilmiyor: "exit 7" ve
**   "exit abc" gibi haller ancak normal hattan gecerek dogru
**   davranabiliyor.
**
** CIKIS KODLARI:
**   Sozdizimi hatasi 2. Calistirma tarafindaki kodlar exec.c icinde
**   yazili. $? hepsini okuyor.
*/

#include "nax.h"
#include "ai.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
** Hatayi kullaniciya bildirir ve verilen cikis kodunu ayarlar.
**
** NEDEN ONCE fflush:
**   stdout boruya yazarken blok tamponlu, stderr ise tamponsuz. Tamponu
**   bosaltmadan hata yazmak, "nax < betik > log 2>&1" gibi bir kullanimda
**   hatalarin ciktidan once gorunmesine yol acar; olculdu.
*/
static void	report_error(t_shell *sh, const char *message, int status)
{
	fflush(stdout);
	if (message == NULL)
		message = "bilinmeyen hata";
	fprintf(stderr, "%s: %s\n", NAX_NAME, message);
	sh->last_status = status;
}

/*
** Token listesinden boru hatti listesini kurar ve calistiriciya verir.
**
** "<<" govdeleri AYRISTIRMADAN SONRA toplaniyor: kac satir okunacagi
** ancak sinirlayici bilindikten sonra belli oluyor. Bu yuzden kabugun
** girdi yolu burada bir satirdan fazlasini okuyabiliyor.
*/
static void	run_tokens(t_shell *sh, const t_token *tokens)
{
	t_ast_err	err;
	t_pipeline	*list;

	list = ast_build(tokens, &err);
	if (list == NULL)
	{
		if (err.message != NULL)
			report_error(sh, err.message, 2);
		return ;
	}
	if (heredoc_fill(list, sh) == 0)
	{
		report_error(sh, "<< govdesi okunamadi", 1);
		ast_free(list);
		return ;
	}
	ex_run(sh, list);
	ast_free(list);
}

/*
** Son calistirilan satiri hatirlar.
**
** NEDEN GEREKLI: tek basina "?" yazildiginda son hatanin aciklanmasi
** isteniyor ve o satirin metni baska yerde tutulmuyor. Bir sonraki
** asamada bu tek satir, baglam halkasina donusecek.
*/
static void	remember_command(t_shell *sh, const char *line)
{
	char	*copy;

	copy = strdup(line);
	if (copy == NULL)
		return ;
	free(sh->last_cmd);
	sh->last_cmd = copy;
}

/*
** Yerel duzeltme onerisini yazar ve uygunsa bir sonraki prompta hazirlar.
**
** ONERI NE ZAMAN TAMPONA KONULMAZ:
**   1  Onerilen komut geri donusu olmayanlardan biriyse. Tampona konan
**      oneri tek Enter'la kosar; "rn -rf ." icin "rm -rf ." hazirlamak
**      refleks bir tusla geri alinamayan silme demek.
**   2  Satir terminalden gelmiyorsa. Betik modunda duzenleme tamponu
**      yok, oneri yalnizca bilgi. Bu kosul TESTLE GOZLEMLENEMEZ: betik
**      modunda on yukleme hicbir sey yapmiyor, yalnizca bir dize
**      ayrilmis kaliyor. Kaldirilmasi davranisi degistirmiyor; niyet
**      belirtmek ve bosa ayirma yapmamak icin duruyor.
**   3  Uzaklik FIX_PRELOAD_DIST'ten buyukse. Oneri yazilir ama
**      hazirlanmaz: iki adim uzaklik cogu zaman parmak kaymasi degil,
**      baska bir sozcuktur. Gerekcenin olcumu ai.h'de yazili.
**   4  Bas satirin basinda aynen gecmiyorsa; fix_rewrite NULL doner.
**
** DURUM KODU 127: hicbir sey kosmadi, yani bash'in "command not found"
** kodu dogru cevap. Oneri yazilmis olmasi bunu degistirmiyor.
*/
static void	report_fix(t_shell *sh, const t_decision *d)
{
	char	*fixed;

	fflush(stdout);
	fprintf(stderr, "%s: %s: boyle bir komut yok\n", NAX_NAME, d->fix.from);
	fprintf(stderr, "%s: bunu mu demek istediniz: %s\n", NAX_NAME,
		d->fix.name);
	sh->last_status = 127;
	if (fix_is_dangerous(d->fix.name) || sh->interactive == 0
		|| d->fix.distance > FIX_PRELOAD_DIST)
		return ;
	fixed = fix_rewrite(d->text, d->fix.from, d->fix.name);
	if (fixed != NULL)
		ln_preload(fixed);
	free(fixed);
}

/* Tek bir girdi satirini isler: siniflandirir, sonra yoluna gonderir. */
static void	handle_line(t_shell *sh, const char *line)
{
	t_lex_err	err;
	t_token		*tokens;
	t_decision	d;

	d = cls_classify(line, &tokens, &err);
	if (d.route == ROUTE_SHELL)
	{
		remember_command(sh, d.text);
		run_tokens(sh, tokens);
		ctx_add(d.text, sh->last_status);
	}
	else if (d.route == ROUTE_FIX)
		report_fix(sh, &d);
	else if (d.route == ROUTE_SYNTAX_ERR)
		report_error(sh, err.message, 2);
	else if (d.route == ROUTE_INTENT)
		ai_intent(sh, d.text);
	else if (d.route == ROUTE_EXPLAIN)
		ai_explain(sh);
	lex_free(tokens);
}

/* Kabuk durumunu ilk degerlerine kurar ve gecmisi yukler. */
static void	shell_init(t_shell *sh)
{
	sh->last_status = 0;
	sh->interactive = isatty(STDIN_FILENO);
	sh->exiting = 0;
	sh->last_cmd = NULL;
	sig_snapshot_inherited();
	if (sh->interactive)
		sig_setup_interactive();
	ln_setup();
	sh->hist_path = ln_hist_path();
	ln_hist_load(sh->hist_path);
}

/*
** Gecmisi diske yazar ve ayrilan tum bellegi birakir.
**
** ln_preload(NULL) SON SATIRDA: kullanici bir oneri aldiktan sonra
** kabuktan cikarsa hazirlanan metin hic tuketilmemis olur. Statik bir
** isaretcide durdugu icin denetleyici bunu sizinti saymiyor, ama yine de
** birakilmamis bir ayirma; projenin kurali sifir sizinti.
*/
static void	shell_free(t_shell *sh)
{
	ai_stop();
	ctx_clear();
	ln_hist_save(sh->hist_path);
	free(sh->hist_path);
	sh->hist_path = NULL;
	free(sh->last_cmd);
	sh->last_cmd = NULL;
	ln_preload(NULL);
}

/*
** Okuma dongusu: satir al, isle, birak. EOF ya da "exit" ile biter.
**
** Kesme kontrolu satir kontrolunden ONCE yapilir: Ctrl-C ile iptal edilen
** satir bos bir dize olarak doner, gercek dosya sonu ise NULL doner. Ikisi
** yalnizca kesme bayragiyla ayrilir.
*/
static void	loop(t_shell *sh)
{
	char	*line;

	while (sh->exiting == 0)
	{
		line = ln_read(sh);
		if (sig_take_interrupt())
		{
			free(line);
			sh->last_status = 130;
			continue ;
		}
		if (line == NULL)
		{
			if (sh->interactive)
				printf("exit\n");
			break ;
		}
		handle_line(sh, line);
		free(line);
	}
}

/* Giris noktasi; kabugun cikis kodu son isin durumudur. */
int	main(void)
{
	t_shell	sh;

	shell_init(&sh);
	loop(&sh);
	shell_free(&sh);
	return (sh.last_status);
}
