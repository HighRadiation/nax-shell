#!/usr/bin/env python3
"""
fake_naxd.py - yardimci surecin yerine gecen, KASTEN KOTU davranan taklit.

NEDEN GERCEK MODELDEN ONCE BU:
    Bu asamanin sorusu "cevap dogru mu" degil, "karsi taraf olur, donar ya
    da sacmalarsa kabuk saglam kaliyor mu". Gercek bir modelle bu
    sorulari uretmek hem yavas hem rastgele; taklitle her biri istege
    gore uretilebiliyor.

KIP ARGUMANLA SECILIR: fake_naxd.py <kip> [gecikme_saniye]

    Gecikme, "slow" ve "slow_ready" kiplerinin bekleme suresi. Testler
    kisa deger veriyor; varsayilan insan olcegine gore secildi.

    ok              Duzgun davranir: READY verir, her INTENT'e OK doner.
    no_ready        Hic READY gondermez; acilis zaman asimina ugramali.
    slow_ready      READY'yi gec gonderir.
    slow            READY verir ama cevaplari gec gonderir; gosterge
                    esigini gecer.
    silent          READY verir, sonra hicbir istege cevap vermez.
    die_at_start    READY'den once cikar.
    die_after_ready READY verir ve hemen cikar.
    die_mid_reply   Cevabin ORTASINDA, yenisatir yazmadan cikar.
    garbage         Cozulemeyen satir gonderir.
    bad_type        Taninmayan tip gonderir.
    wrong_id        Baska bir kimlikle cevap verir; sessizce atilmali.
    stale_then_ok   Once BIR ONCEKI kimlikle, sonra dogru kimlikle cevap
                    verir; ilki atilmali.
    huge            Bir mebibayttan uzun satir gonderir.
    noisy_stderr    Hata cikisina bol bol yazar; terminale sizmamali.
    deaf            READY verir, sonra stdin'i HIC OKUMAZ. Boru dolunca
                    kabugun yazmasi tikanir; sinirli beklememesi
                    kilitlenme demek olurdu.
    echo_request    Kabuktan gelen tipi AYNEN geri gonderir. Gecerli ama
                    yanlis YONDE bir tip; ihlal sayilmali.
    answer          INTENT'e SORU cevabi doner (kind=question), komut degil.
    cmd_echo        Belirli bir echo komutu onerir; on yukleme olcumu icin.
    risky_cmd       Riskli bir komut onerir ve danger=1 der.
    risky_lie       Riskli bir komut onerir ama danger=0 DER. Kabuk karsi
                    tarafa guvenmeyip kendi listesine bakmali.
    nogo            READY'de AI'in kapali oldugu dizin listesini bildirir;
                    liste NAX_FAKE_NOGO ortam degiskeninden okunur.
    silent_fast     READY'de KISA sure bildirir, sonra hic cevap vermez.
                    Kabugun el sikismada bildirilen zaman asimini gercekten
                    uyguladigini olcmek icin.
    late_reader     READY verir, bir sure BEKLER, sonra normal okur. Boru
                    gecici olarak dolar; bu baglantiyi OLDURMEMELI.

CIKTI TAMPONLANMAZ: her satir hemen gonderilir, yoksa kabuk bekledigi
cevabi tamponda kalmis olabilir diye zaman asimina ugrardi.
"""

import base64
import os
import sys
import time


def send(line):
    sys.stdout.write(line + "\n")
    sys.stdout.flush()


def b64(text):
    return "b64:" + base64.b64encode(text.encode()).decode()


def read_frames():
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line:
            continue
        parts = line.split("\t")
        fields = {}
        for part in parts[1:]:
            if "=" in part:
                key, value = part.split("=", 1)
                fields[key] = value
        yield parts[0], fields


def mode_ok(kind, fields):
    ident = fields.get("id", "0")
    if kind == "INTENT":
        send("OK\tid=%s\tkind=request\tcmd=%s\tdanger=0"
             % (ident, b64("ls -la")))
    elif kind == "EXPLAIN":
        send("OK\tid=%s\tkind=question\ttext=%s"
             % (ident, b64("komut bulunamadi")))
    elif kind == "BYE":
        sys.exit(0)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "ok"
    delay = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0

    if mode == "die_at_start":
        sys.exit(3)
    if mode == "noisy_stderr":
        for i in range(200):
            sys.stderr.write("taklit gurultu satiri %d\n" % i)
        sys.stderr.flush()
    if mode == "slow_ready":
        time.sleep(delay)
    if mode == "nogo":
        send("READY\tid=0\tversion=1\tkey=yes\tnogo=%s"
             % os.environ.get("NAX_FAKE_NOGO", "/yok"))
    elif mode == "silent_fast":
        send("READY\tid=0\tversion=1\tkey=yes\ttimeout=1\tspinner=0.3")
    elif mode != "no_ready":
        send("READY\tid=0\tversion=1\tkey=taklit")
    if mode == "die_after_ready":
        sys.exit(0)

    if mode == "deaf":
        time.sleep(60)
        return
    if mode == "late_reader":
        time.sleep(delay)

    for kind, fields in read_frames():
        ident = fields.get("id", "0")
        if kind == "BYE":
            return
        if kind == "CANCEL":
            continue
        if mode in ("silent", "no_ready", "silent_fast"):
            continue
        if mode == "cmd_echo":
            send("OK\tid=%s\tkind=request\tcmd=%s\tdanger=0"
                 % (ident, b64("echo ai-onyukleme-kaniti")))
            continue
        if mode == "risky_cmd":
            send("OK\tid=%s\tkind=request\tcmd=%s\tdanger=1"
                 % (ident, b64("rm -rf /tmp/kazadan-sonra")))
            continue
        if mode == "risky_lie":
            send("OK\tid=%s\tkind=request\tcmd=%s\tdanger=0"
                 % (ident, b64("rm -rf /tmp/yalan-soyleyen")))
            continue
        if mode == "answer":
            send("OK\tid=%s\tkind=question\ttext=%s"
                 % (ident, b64("bu bir dizin listesi")))
            continue
        if mode == "die_mid_reply":
            sys.stdout.write("OK\tid=%s\tkind=request\tcmd=b64:" % ident)
            sys.stdout.flush()
            sys.exit(0)
        if mode == "garbage":
            send("bu satir hicbir seye benzemiyor")
            continue
        if mode == "bad_type":
            send("WAT\tid=%s" % ident)
            continue
        if mode == "echo_request":
            send("%s\tid=%s" % (kind, ident))
            continue
        if mode == "wrong_id":
            send("OK\tid=9999\tkind=request\tcmd=%s\tdanger=0" % b64("ls"))
            continue
        if mode == "stale_then_ok":
            # Eski kimlik GELEN kimlikten turetiliyor. Sabit "1" yazmak
            # ilk istegin kimligiyle cakisiyordu, yani "eski" cevap
            # dogru cevap sayiliyordu ve vaka hicbir sey olcmuyordu.
            send("OK\tid=%d\tkind=request\tcmd=%s\tdanger=0"
                 % (int(ident) - 1, b64("eski")))
            send("OK\tid=%s\tkind=request\tcmd=%s\tdanger=0"
                 % (ident, b64("yeni")))
            continue
        if mode == "huge":
            send("OK\tid=%s\tkind=question\ttext=b64:%s"
                 % (ident, "QQ" * 600000))
            continue
        if mode == "slow":
            time.sleep(delay)
        mode_ok(kind, fields)


if __name__ == "__main__":
    main()
