#!/usr/bin/env python3
"""
unit_wire.py - tel biciminin iki dilde AYNI olmasini dogrular.

NEDEN AYNI KORPUS:
    Kabuk C, yardimci surec Python. Iki taraf ayni bicimi konusmak
    zorunda ve kurallar ayrisirsa birbirlerini SESSIZCE yanlis okurlar -
    ayiklanmasi en zor hata turu. Bu test C tarafinin vaka dosyasini
    (test/cases/proto.tsv) oldugu gibi okur ve Python tarafina ayni
    beklentileri uygular.

    Yani korpus artik tek bir uygulamanin testi degil, BICIMIN kendisinin
    testi. Bir tarafta kural degisirse digeri patlar.

VAKA TURLERI (bicim test_proto.c basliginda anlatiliyor):
    P:<satir>   cozme; beklenen kanonik dokum ya da "ERR:<ad>"
    B:TIP|...   kurma; beklenen tam tel satiri
    C:<ad>      C tarafina ozel kontrol, burada ATLANIR

HATA ADLARI KARSILASTIRILMAZ: C tarafi PE_BAD_B64 gibi ayrik degerler
uretiyor, Python ValueError yukseltiyor. Olculen sey ikisinin AYNI
girdileri reddetmesi; hata isimlerini esitlemek iki tarafi gereksiz yere
birbirine baglardi.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "naxd"))
sys.path.insert(0, os.path.dirname(__file__))

import pyharness
import wire


def show_value(value):
    """Denetim karakterlerini C tarafiyla ayni etiketlerle gosterir."""
    out = []
    for char in value:
        if char == "\t":
            out.append("<TAB>")
        elif char == "\n":
            out.append("<NL>")
        elif ord(char) < 0x20:
            out.append("<%02x>" % ord(char))
        else:
            out.append(char)
    return "".join(out)


def dump(kind, fields):
    """Cozulmus kaydi kanonik dokume cevirir."""
    parts = [kind]
    for key, value in fields.items():
        parts.append("%s=%s" % (key, show_value(value)))
    return "|".join(parts)


def case_parse(report, number, given, want):
    """Cozme vakasi; hata beklenen vakalarda yalnizca reddedilmesi aranir."""
    try:
        kind, fields = wire.parse(given)
    except ValueError as exc:
        got = "ERR"
        if want.startswith("ERR:"):
            report.ok()
        else:
            report.bad(number, given, want, "%s (%s)" % (got, exc))
        return
    got = dump(kind, fields)
    if want.startswith("ERR:"):
        report.bad(number, given, want, got)
    else:
        report.judge(number, given, want, got)


def case_build(report, number, given, want):
    """Kurma vakasi; belirtimi ayirip satiri kurar."""
    parts = given.split("|")
    kind = parts[0]
    ident = "0"
    fields = {}
    if len(parts) > 1 and parts[1].startswith("id="):
        ident = parts[1][3:]
    for part in parts[2:]:
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        fields[key] = value
    if want == "NULL":
        report.ok()
        return
    got = wire.build(kind, ident, fields)
    if not got.endswith("\n") or got.count("\n") != 1:
        report.bad(number, given, want, "yenisatir kurali bozuk")
        return
    report.judge(number, given, want, got[:-1])


def run_case(report, number, given, want):
    """Vakayi onekine gore yonlendirir; C'ye ozel kontroller atlanir."""
    if given.startswith("P:"):
        case_parse(report, number, given[2:], want)
    elif given.startswith("B:"):
        case_build(report, number, given[2:], want)


if __name__ == "__main__":
    sys.exit(pyharness.main_for("test/cases/proto.tsv",
                                "wire testleri (C korpusu)", run_case))
