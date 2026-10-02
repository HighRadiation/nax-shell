"""
wire.py - tel biciminin Python tarafi.

C tarafindaki src/ai/proto.c ile BIRE BIR ayni kurallari uygulamak
zorunda. Kurallar ayrisirsa iki taraf birbirini sessizce yanlis okur ve
bu, ayiklanmasi en zor hata turudur.

AYNI OLMASI GEREKEN UC KURAL:

  1  Satir bicimi: <TIP>\\tid=<n>\\tanahtar=deger\\n ve en fazla on alti alan
  2  Deger "sade" ise oldugu gibi gider: bos degil, "b64:" ile baslamiyor
     ve yalnizca harf, rakam ve "_.:/-," iceriyor.
  3  "b64:" ile BASLAYAN deger kodlanmak zorunda, yoksa karsi taraf onu
     kodlanmis sanip bozuk veri uretir.

Taninan tip listesi de sozlesmenin parcasi: iki taraf ayni kumeyi
bilmezse biri digerinin gonderdigini reddeder. Caprazlama test bunu
olcuyor ve ilk yazimda uc ayrisma yakaladi - Python tarafi bilinmeyen
tipi ve on altidan fazla alani kabul ediyordu.

Bicimin gerekcesi docs/PROTOCOL.md icinde.
"""

import base64

PLAIN_EXTRA = "_.:/-,"
MAX_LINE = 1048576
MAX_FIELDS = 16

# Taninan kayit tipleri.
#
# Liste C tarafindaki t_ftype ile ayni olmak zorunda. Iki yerde durmasi
# tekrar gibi gorunuyor ama bu bir SOZLESME: taninan tipler kumesi
# bicimin parcasi ve iki uygulamanin ayni kumeyi bilmesi gerekiyor.
# Caprazlama test (test/unit_wire.py) ayrismayi yakaliyor.
KNOWN_KINDS = (
    "HELLO", "INTENT", "EXPLAIN", "EVENT", "CANCEL", "BYE",
    "READY", "OK", "NEED", "ERR",
)


def is_plain(value):
    """Deger oldugu gibi gonderilebilir mi."""
    if not value:
        return False
    if value.startswith("b64:"):
        return False
    for char in value:
        if not (char.isascii() and (char.isalnum() or char in PLAIN_EXTRA)):
            return False
    return True


def encode_value(value):
    """Degeri sade ya da kodlanmis halde dondurur."""
    if value is None:
        value = ""
    if is_plain(value):
        return value
    return "b64:" + base64.b64encode(value.encode("utf-8")).decode("ascii")


def build(kind, ident, fields=None):
    """Bir kayit satiri kurar; sonunda yenisatir vardir."""
    parts = ["%s\tid=%s" % (kind, ident)]
    for key, value in (fields or {}).items():
        parts.append("%s=%s" % (key, encode_value(value)))
    return "\t".join(parts) + "\n"


def decode_value(text):
    """Kodlanmis degeri cozer; bozuksa ValueError yukseltir."""
    if not text.startswith("b64:"):
        return text
    raw = text[4:]
    if len(raw) % 4 != 0:
        raise ValueError("base64 uzunlugu dordun kati degil")
    try:
        data = base64.b64decode(raw, validate=True)
    except Exception as exc:
        raise ValueError("base64 cozulemedi") from exc
    if b"\x00" in data:
        raise ValueError("cozulen veride gomulu NUL")
    return data.decode("utf-8", "replace")


def parse(line):
    """
    Bir kayit satirini (tip, alanlar) ciftine cevirir.

    Bozuk satirda ValueError yukseltir. Tolerans GOSTERILMEZ: bozuk bir
    cerceve karsi tarafin saglikli olmadiginin isaretidir ve sessizce
    kabul etmek sorunu gizlerdi.
    """
    line = line.rstrip("\n")
    if len(line) > MAX_LINE:
        raise ValueError("satir asiri uzun")
    if not line:
        raise ValueError("bos satir")
    parts = line.split("\t")
    kind = parts[0]
    if kind not in KNOWN_KINDS:
        raise ValueError("taninmayan tip: %r" % kind)
    if len(parts) - 1 > MAX_FIELDS:
        raise ValueError("alan sayisi siniri asildi")
    fields = {}
    for part in parts[1:]:
        if "=" not in part:
            raise ValueError("esittir isareti yok: %r" % part)
        key, value = part.split("=", 1)
        if not key or not all(c.isalnum() or c == "_" for c in key):
            raise ValueError("gecersiz anahtar: %r" % key)
        fields[key] = decode_value(value)
    return kind, fields
