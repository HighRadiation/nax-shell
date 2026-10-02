#!/usr/bin/env python3
"""
unit_naxd.py - gercek yardimci surecin ucu uca davranisi.

NE OLCULUYOR:
    Yapilandirma okuma, tel bicimi, model cagrisi, cevap ayristirma ve
    dusme kurali bir arada. Karsi tarafta sahte bir model servisi var
    (test/stub_provider.py), yani ag, anahtar ve ucret gerekmiyor.

    Surec GERCEK surec: kabugun baslattigi komutun aynisi calisiyor. Bu
    yuzden test ayni zamanda "Python tarafi C tarafinin bekledigi
    bicimde konusuyor mu" sorusunu da yanitliyor.

GIRDI BICIMI:
    <kip>[@saglayici][:tur]
                  Kip sahte servisin model adi olarak gonderilir ve onun
                  hangi cevabi dondurecegini secer. Saglayici "anthropic"
                  yazilirsa Claude bagdastiricisi kullanilir; yazilmazsa
                  uyumlu bagdastirici. tur "intent" (ontanimli) ya da
                  "explain".
    C:<ad>        Tabloda anlatilamayan adli kontrol.

BEKLENEN BICIMI:
    ";" ile ayrilmis olaylar:
      READY:yes | READY:no      Acilis bildirimi ve anahtarin varligi
      OK:request:<komut>:<risk> Komut onerisi
      OK:question:<metin>       Soru cevabi
      ERR:<kod>                 Hata
"""

import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "naxd"))

import config as config_mod
import provider as provider_mod
import pyharness
import stub_provider
import wire

CONF_TEMPLATE = """provider = %(provider)s
base_url = %(base)s
api_key = %(key)s
model_intent = %(mode)s
model_explain = %(mode)s
use_local = false
local_url = %(base)s
local_model = local
local_fallback = true
timeout = 5
spinner = 1
"""


class quiet_log:
    """
    Gunluk satirlarini yutar.

    NEDEN GEREKLI: bazi kontroller saglayiciyi bu surecin icinde cagiriyor,
    yani onun gunluk satirlari testin ciktisina karisiyor. Uretimde o akis
    gunluk dosyasina gidiyor; testte de gorunmemesi gerekiyor, yoksa
    gercek bir hata mesaji gurultunun arasinda kaybolur.
    """

    def __enter__(self):
        self.saved = sys.stderr
        sys.stderr = open(os.devnull, "w", encoding="utf-8")
        return self

    def __exit__(self, *unused):
        sys.stderr.close()
        sys.stderr = self.saved
        return False


def write_conf(base, mode, key="testanahtari", extra="", provider="groq"):
    """Gecici bir yapilandirma dosyasi yazar ve yolunu dondurur."""
    handle = tempfile.NamedTemporaryFile("w", suffix=".conf", delete=False,
                                         encoding="utf-8")
    handle.write(CONF_TEMPLATE % {"base": base, "mode": mode, "key": key,
                                  "provider": provider})
    handle.write(extra)
    handle.close()
    return handle.name


def run_daemon(conf_path, frames):
    """
    Gercek yardimci sureci baslatip verilen kayitlari gonderir.

    Donen sey surecin yazdigi satirlarin listesi. Hata cikisi ayri
    tutuluyor: kabuk da onu gunluge yonlendiriyor ve testin ciktisini
    kirletmemesi gerekiyor.
    """
    env = dict(os.environ)
    env["NAX_CONF"] = conf_path
    proc = subprocess.Popen(
        [sys.executable, os.path.join(ROOT, "naxd", "naxd.py")],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, text=True, cwd=ROOT, env=env)
    out, _err = proc.communicate("".join(frames) + wire.build("BYE", 99),
                                 timeout=30)
    return [line for line in out.splitlines() if line.strip()]


def describe(lines):
    """Surecin yazdigi satirlari olay dizisine cevirir."""
    events = []
    for line in lines:
        try:
            kind, fields = wire.parse(line)
        except ValueError as exc:
            events.append("COZULEMEDI(%s)" % exc)
            continue
        if kind == "READY":
            events.append("READY:%s" % fields.get("key", "?"))
        elif kind == "OK" and fields.get("kind") == "request":
            events.append("OK:request:%s:%s"
                          % (fields.get("cmd", ""), fields.get("danger", "")))
        elif kind == "OK":
            events.append("OK:question:%s" % fields.get("text", ""))
        elif kind == "ERR":
            events.append("ERR:%s" % fields.get("code", ""))
        else:
            events.append(kind)
    return ";".join(events)


def case_play(report, number, given, want):
    """Senaryo vakasi: kipi secip tek bir istek gonderir."""
    mode, _, task = given.partition(":")
    provider = "groq"
    if "@" in mode:
        mode, _, provider = mode.partition("@")
    task = task or "intent"
    server, base = stub_provider.start()
    key = "" if mode == "nokey" else "testanahtari"
    conf = write_conf(base, mode, key=key, provider=provider)
    try:
        if task == "explain":
            frame = wire.build("EXPLAIN", 1,
                               {"cmd": "celar", "code": "127"})
        else:
            frame = wire.build("INTENT", 1, {"text": "dun degisen dosyalar"})
        got = describe(run_daemon(conf, [frame]))
    finally:
        server.shutdown()
        os.unlink(conf)
    report.judge(number, given, want, got)


def check_config_reload():
    """Dosya degisince yeni ayar gecerli olmali; yeniden baslatma yok."""
    server, base = stub_provider.start()
    conf = write_conf(base, "ok")
    try:
        cfg = config_mod.Config(conf)
        before = cfg.text("model_intent")
        with open(conf, "w", encoding="utf-8") as handle:
            handle.write(CONF_TEMPLATE
                         % {"base": base, "mode": "degisti", "key": "k",
                            "provider": "groq"})
        os.utime(conf, (0, 0))
        changed = cfg.refresh()
        after = cfg.text("model_intent")
    finally:
        server.shutdown()
        os.unlink(conf)
    return before == "ok" and changed and after == "degisti"


def check_example_keys_known():
    """
    nax.conf.example icindeki her anahtar taninmali.

    Ornege yeni bir alan eklenip koda eklenmezse o alan sessizce yok
    sayilirdi - kullanici yazdigi ayarin neden ise yaramadigini goremezdi.
    """
    unknown = []
    with open(os.path.join(ROOT, "nax.conf.example"), encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key = line.split("=", 1)[0].strip()
            if key not in config_mod.DEFAULTS:
                unknown.append(key)
    return not unknown


def check_unknown_key_warns():
    """Taninmayan anahtar sessizce yok sayilmamali."""
    seen = []
    with quiet_log():
        values = config_mod.parse_text(
            "saglayici = groq\nprovider = openai\n", seen.append)
    return values == {"provider": "openai"} and len(seen) == 1


def check_fallback_after_two_failures():
    """
    Ust uste iki bulut hatasindan sonra yerel modele gecilir.

    Tetikleyici "internet var mi" degil "cagri patladi mi". Sahte servis
    bulut icin hata, yerel icin gecerli cevap donduruyor; boylece gecisin
    gercekten yasandigi cevabin ICERIGINDEN goruluyor.
    """
    server, base = stub_provider.start()
    conf = write_conf(base, "http500")
    try:
        with quiet_log():
            cfg = config_mod.Config(conf)
            prov = provider_mod.Provider(cfg)
            fails = 0
            for _ in range(2):
                try:
                    prov.ask("intent", [{"role": "user", "content": "x"}])
                except provider_mod.ProviderError:
                    fails += 1
            after = prov.ask("intent", [{"role": "user", "content": "x"}])
    finally:
        server.shutdown()
        os.unlink(conf)
    return fails == 2 and "yerelden gelen" in after


def check_success_resets_counter():
    """Basarili bir bulut cagrisi sayaci sifirlar."""
    server, base = stub_provider.start()
    conf = write_conf(base, "http500")
    try:
        with quiet_log():
            cfg = config_mod.Config(conf)
            prov = provider_mod.Provider(cfg)
            try:
                prov.ask("intent", [{"role": "user", "content": "x"}])
            except provider_mod.ProviderError:
                pass
            first = prov.cloud_fails
            cfg.values["model_intent"] = "ok"
            prov.ask("intent", [{"role": "user", "content": "x"}])
            after = prov.cloud_fails
    finally:
        server.shutdown()
        os.unlink(conf)
    return first == 1 and after == 0


def check_key_reaches_header():
    """Anahtar istek basligina Bearer olarak konulmali."""
    server, base = stub_provider.start()
    conf = write_conf(base, "ok", key="gizlianahtar")
    try:
        cfg = config_mod.Config(conf)
        prov = provider_mod.Provider(cfg)
        prov.ask("intent", [{"role": "user", "content": "x"}])
        auth = server.seen[-1]["auth"]
        warmth = server.seen[-1]["temperature"]
    finally:
        server.shutdown()
        os.unlink(conf)
    return auth == "Bearer gizlianahtar" and warmth == 0.0


def check_intent_text_reaches_model():
    """Kullanicinin yazdigi metin modele oldugu gibi gitmeli."""
    server, base = stub_provider.start()
    conf = write_conf(base, "echo")
    try:
        frame = wire.build("INTENT", 1, {"text": "sirket dosyasini bul"})
        got = describe(run_daemon(conf, [frame]))
    finally:
        server.shutdown()
        os.unlink(conf)
    return got == "READY:yes;OK:request:echo sirket dosyasini bul:0"


def check_anthropic_shape():
    """
    Claude istegi dogru bicimde gidiyor mu.

    Uc sey olculuyor ve ucu de zorunlu:
      - uc nokta "/messages", anahtar "x-api-key" basliginda, surum basligi
        var
      - sistem istemi UST DUZEY "system" alaninda, mesaj listesinde degil
      - SICAKLIK GONDERILMIYOR: guncel modellerde gonderilmesi istegi 400
        yapiyor, yani uyumlu bagdastiricinin govdesi oldugu gibi
        kullanilamaz
    """
    server, base = stub_provider.start()
    conf = write_conf(base, "ok", key="gizli", provider="anthropic")
    try:
        with quiet_log():
            cfg = config_mod.Config(conf)
            prov = provider_mod.Provider(cfg)
            text = prov.ask("intent", [{"role": "system", "content": "SIS"},
                                       {"role": "user", "content": "merhaba"}])
        seen = server.seen[-1]
    finally:
        server.shutdown()
        os.unlink(conf)
    ok = seen["path"].endswith("/messages")
    ok = ok and seen["api_key"] == "gizli" and seen["auth"] == ""
    ok = ok and seen["version"] == "2023-06-01"
    ok = ok and seen["system"] == "SIS"
    ok = ok and all(m.get("role") != "system" for m in seen["messages"])
    ok = ok and seen["temperature"] is None
    ok = ok and isinstance(seen["max_tokens"], int)
    return ok and text == "CMD ls -la"


def check_anthropic_refusal():
    """
    Reddedilen istek hata sayilmali.

    Red HTTP 200 ile geliyor, yani durum koduna bakmak yetmiyor;
    "stop_reason" denetlenmek zorunda. Denetlenmezse bos bir cevap
    kullaniciya "bos komut" olarak gider ve sebebi hic gorunmez.
    """
    server, base = stub_provider.start()
    conf = write_conf(base, "refused", provider="anthropic")
    try:
        with quiet_log():
            cfg = config_mod.Config(conf)
            prov = provider_mod.Provider(cfg)
            try:
                prov.ask("intent", [{"role": "user", "content": "x"}])
                return False
            except provider_mod.ProviderError as exc:
                return "reddedildi" in str(exc)
    finally:
        server.shutdown()
        os.unlink(conf)


def check_local_uses_compat():
    """
    Dusme yolu her zaman UYUMLU bicimi kullanir.

    Saglayici "anthropic" olsa bile yerel sunucular uyumlu bicimi
    konusuyor. Yerel cagriyi Claude bicimiyle yapmak, dusmenin hic
    calismamasi demek olurdu.
    """
    server, base = stub_provider.start()
    conf = write_conf(base, "http500", provider="anthropic")
    try:
        with quiet_log():
            cfg = config_mod.Config(conf)
            prov = provider_mod.Provider(cfg)
            for _ in range(2):
                try:
                    prov.ask("intent", [{"role": "user", "content": "x"}])
                except provider_mod.ProviderError:
                    pass
            text = prov.ask("intent", [{"role": "user", "content": "x"}])
        seen = server.seen[-1]
    finally:
        server.shutdown()
        os.unlink(conf)
    return seen["path"].endswith("/chat/completions") and "yerelden" in text


def check_user_agent_sent():
    """
    Istek kendi imzasiyla gitmeli, urllib'in varsayilaniyla degil.

    Varsayilan "Python-urllib/3.x" imzasi Cloudflare'in bot listesinde ve
    istek API'ye hic ulasmadan 403 doner. Bu kontrol gercek bir arizadan
    dogdu; imza basligi kazara dusurulurse AI yolu tamamen durur.
    """
    server, base = stub_provider.start()
    conf = write_conf(base, "ok")
    try:
        cfg = config_mod.Config(conf)
        prov = provider_mod.Provider(cfg)
        prov.ask("intent", [{"role": "user", "content": "x"}])
        agent = server.seen[-1]["agent"]
    finally:
        server.shutdown()
        os.unlink(conf)
    return agent == provider_mod.USER_AGENT and "urllib" not in agent.lower()


def check_error_reason_reaches_user():
    """
    Saglayicinin gerekcesi kullaniciya gosterilen satira girmeli.

    Yalnizca durum kodunu gostermek ("servis 403 dondurdu") kullaniciyi
    yanlis yere bakmaya itiyor: 403 gorunce insan ilk is anahtarini
    sucluyor. Govde JSON degil duz metin oldugu halde sebep tasinmali.
    """
    server, base = stub_provider.start()
    conf = write_conf(base, "cf1010", extra="local_fallback = false\n")
    try:
        with quiet_log():
            cfg = config_mod.Config(conf)
            prov = provider_mod.Provider(cfg)
            try:
                prov.ask("intent", [{"role": "user", "content": "x"}])
                return False
            except provider_mod.ProviderError as exc:
                return "403" in str(exc) and "1010" in str(exc)
    finally:
        server.shutdown()
        os.unlink(conf)


def check_json_error_reason_unwrapped():
    """
    JSON govdede sebep "error.message" alanindan cikarilmali.

    Govdeyi oldugu gibi basmak kullaniciya kullanilmaz bir JSON parcasi
    gosterirdi; alan adlarini sokmek de sebebi kaybetmek olurdu.
    """
    reason = provider_mod.error_reason(
        '{"error":{"message":"Invalid API Key","type":"invalid_request"}}')
    uzun = provider_mod.error_reason("x" * 200)
    return reason == "Invalid API Key" and len(uzun) == 80


CHECKS = {
    "user_agent_sent": check_user_agent_sent,
    "error_reason_reaches_user": check_error_reason_reaches_user,
    "json_error_reason_unwrapped": check_json_error_reason_unwrapped,
    "anthropic_shape": check_anthropic_shape,
    "anthropic_refusal": check_anthropic_refusal,
    "local_uses_compat": check_local_uses_compat,
    "config_reload": check_config_reload,
    "example_keys_known": check_example_keys_known,
    "unknown_key_warns": check_unknown_key_warns,
    "fallback_after_two_failures": check_fallback_after_two_failures,
    "success_resets_counter": check_success_resets_counter,
    "key_reaches_header": check_key_reaches_header,
    "intent_text_reaches_model": check_intent_text_reaches_model,
}


def case_check(report, number, given, want):
    """Adli kontrolu kosar."""
    runner = CHECKS.get(given)
    if runner is None:
        report.bad(number, given, want, "boyle bir kontrol yok")
        return
    try:
        ok = runner()
    except Exception as exc:
        report.bad(number, given, want, "patladi: %r" % (exc,))
        return
    report.judge(number, given, want, "ok" if ok else "kontrol basarisiz")


def run_case(report, number, given, want):
    """Vakayi onekine gore yonlendirir."""
    if given.startswith("C:"):
        case_check(report, number, given[2:], want)
    else:
        case_play(report, number, given, want)


if __name__ == "__main__":
    sys.exit(pyharness.main_for("test/cases/naxd_py.tsv",
                                "naxd (gercek surec) testleri", run_case))
