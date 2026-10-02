#!/usr/bin/env python3
"""
naxd.py - yardimci surec: kabuktan gelen niyeti modele goturur.

YALNIZCA STANDART KUTUPHANE:
    Kullanicinin paket kurmasi gerekmemeli. Kabuk zaten derleniyor;
    uzerine bir de Python bagimliligi eklemek kurulumu iki katina
    cikarirdi.

TEK BEKLEYEN ISTEK, TEK IS PARCACIGI:
    Kabuk zaten cevabi bekliyor, yani ayni anda iki istek havada olmaz.
    Bu kural burada da gecerli: dongu bir satir okur, cevaplar, sonrakine
    gecer. Es zamanlilik yok, dolayisiyla yaris da yok.

CIKTI TAMPONLANMAZ:
    Her kayit yazildigi an gonderilir. Tamponda kalan bir cevap kabugun
    zaman asimina ugramasina yol acar ve sebebi hic gorunmez.

HATA CIKISI GUNLUGE GIDER:
    Kabuk bu sureci baslatirken stderr'i bir dosyaya yonlendiriyor.
    Buradan yazilan her sey oraya gider: kullanicinin ekrani bozulmaz ama
    ayrinti da kaybolmaz.

AI'IN KAPALI OLDUGU DIZINLER DE EL SIKISMADA GIDER:
    "Hic gonderme" karari GONDEREN tarafta olmak zorunda; listeyi burada
    tutup kabuga bildirmek, karari dogru tarafta birakirken yapilandirmayi
    tek kaynakta tutuyor.

SURELER EL SIKISMADA BILDIRILIR:
    timeout ve spinner yapilandirmada duruyor ama yapilandirmayi yalnizca
    bu taraf okuyor. Kabugun da bu surelere ihtiyaci var: beklemeyi o
    yapiyor. Degerleri READY kaydiyla gondermek celiskiyi cozuyor - C
    tarafinda ayristirici yazmak gerekmiyor ve tek kaynak korunuyor.

CANCEL NE YAPAR:
    Dongu tek is parcacikli oldugu icin CANCEL ancak biz cevabi
    gonderdikten SONRA okunabilir - model cagrisi sirasinda stdin
    okunmuyor. Bu yuzden yapilacak bir sey yok ve kayit sessizce
    gecilir. Kabuk tarafi zaten gec gelen cevabi kimligine bakip atiyor,
    yani dogruluk buna bagli degil.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import config as config_mod
import prompt as prompt_mod
import provider as provider_mod
import wire

VERSION = "1"

# Geri donusu olmayan komutlar.
#
# Bu liste YALNIZCA bir ipucu uretir (danger alani). Asil karari kabuk
# kendi listesiyle veriyor ve oneriyi duzenleme satirina koyup
# koymayacagina kendisi karar veriyor. Guvenligi karsi tarafin dogru
# cevap vermesine baglamak yanlis olurdu.
RISKY = (
    "rm", "rmdir", "dd", "shred", "mkfs", "mkswap", "fdisk", "parted",
    "chown", "chgrp", "chmod", "kill", "killall", "pkill", "mv",
    "truncate", "reboot", "shutdown", "halt", "poweroff", "userdel",
    "groupdel",
)


def send(line):
    """Kaydi hemen gonderir."""
    sys.stdout.write(line)
    sys.stdout.flush()


def log(message):
    """Gunluge yazar; kullanicinin ekranina degil."""
    sys.stderr.write("naxd: %s\n" % message)
    sys.stderr.flush()


def looks_risky(command):
    """Komutun basi geri donusu olmayanlardan biri mi."""
    head = command.strip().split(" ")[0] if command.strip() else ""
    head = head.rsplit("/", 1)[-1]
    if head in RISKY:
        return True
    return head.startswith("mkfs.")


def answer_intent(cfg, prov, ident, fields):
    """Niyeti cevaplar: ya komut onerir ya soruyu yanitlar."""
    text = fields.get("text", "").strip()
    if not text:
        return wire.build("ERR", ident, {"code": "bos", "message": "bos niyet"})
    context = fields.get("context", "")
    try:
        reply = prov.ask("intent", prompt_mod.intent_messages(text, context))
    except provider_mod.ProviderError as exc:
        return wire.build("ERR", ident,
                          {"code": "saglayici", "message": str(exc)})
    kind, body = prompt_mod.read_reply(reply)
    if kind == "request":
        return wire.build("OK", ident, {
            "kind": "request",
            "cmd": body,
            "danger": "1" if looks_risky(body) else "0",
        })
    return wire.build("OK", ident, {"kind": "question", "text": body})


def answer_explain(cfg, prov, ident, fields):
    """Basarisiz komutu aciklar."""
    command = fields.get("cmd", "").strip()
    if not command:
        return wire.build("ERR", ident,
                          {"code": "bos", "message": "aciklanacak komut yok"})
    try:
        reply = prov.ask("explain", prompt_mod.explain_messages(
            command, fields.get("code", ""), fields.get("message", "")))
    except provider_mod.ProviderError as exc:
        return wire.build("ERR", ident,
                          {"code": "saglayici", "message": str(exc)})
    return wire.build("OK", ident,
                      {"kind": "question", "text": reply.strip()})


def handle(cfg, prov, kind, fields):
    """
    Bir kaydi isler ve gonderilecek cevabi dondurur; cevapsizsa None.

    HELLO ve EVENT cevap uretmez: ilki oturum anlik goruntusu, ikincisi
    kosan komutun bildirimi. Ikisi de bir sonraki asamada baglam halkasini
    besleyecek; su an alinip gunluge yazilmakla kaliyorlar.
    """
    ident = fields.get("id", "0")
    if kind == "BYE":
        return None
    if kind == "CANCEL":
        return None
    if kind == "HELLO":
        log("oturum: %s" % fields.get("cwd", "(bilinmiyor)"))
        return None
    if kind == "EVENT":
        return None
    if kind == "INTENT":
        return answer_intent(cfg, prov, ident, fields)
    if kind == "EXPLAIN":
        return answer_explain(cfg, prov, ident, fields)
    return wire.build("ERR", ident,
                      {"code": "tip", "message": "beklenmeyen tip: %s" % kind})


def main():
    """Acilisi bildirir ve kayitlari sirayla isler."""
    cfg = config_mod.Config()
    prov = provider_mod.Provider(cfg)
    send(wire.build("READY", 0, {
        "version": VERSION,
        "key": "yes" if cfg.has_key() else "no",
        "timeout": "%g" % cfg.number("timeout"),
        "spinner": "%g" % cfg.number("spinner"),
        "nogo": cfg.text("ai_off_dirs"),
    }))
    if not cfg.has_key():
        log("api_key bos: AI kapali, kabuk duz kabuk olarak calisir")
    for raw in sys.stdin:
        cfg.refresh()
        try:
            kind, fields = wire.parse(raw)
        except ValueError as exc:
            log("bozuk kayit atlandi: %s" % exc)
            continue
        if kind == "BYE":
            return 0
        reply = handle(cfg, prov, kind, fields)
        if reply is not None:
            send(reply)
    return 0


if __name__ == "__main__":
    sys.exit(main())
