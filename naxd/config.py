"""
config.py - nax.conf okuma.

NEDEN YALNIZ BURADA:
    Yapilandirmayi yalnizca yardimci surec okur, kabuk (C tarafi)
    okumaz. Boylece ayarlar tek bir yerde kaliyor ve C tarafina
    ayristirici yazmak gerekmiyor. Gerekcesi docs/CONFIG.md icinde.

HER ISTEKTE DEGISIKLIK KONTROLU:
    Kullanici dosyayi kaydettigi an yeni ayar gecerli olmali; kabugu
    yeniden baslatmak gerekmemeli. Kontrol degisme zamani VE boyut
    uzerinden yapiliyor: yalnizca zamana bakmak, ayni saniye icinde
    yapilan iki degisikligi kacirabilir.

TANINMAYAN ANAHTAR SESSIZCE YOK SAYILMAZ:
    Yanlis yazilmis bir anahtarin sessizce yok sayilmasi en sinir bozucu
    hata turudur - kullanici "use_local yazdim ama bir sey olmadi" der ve
    sebebini goremez. Taninmayan anahtar hata cikisina yazilir, o da
    gunluk dosyasina gider. Olumcul DEGIL: bir yazim hatasi yuzunden AI'i
    tamamen kapatmak daha kotu olurdu.
"""

import os
import sys

# Taninan alanlar ve varsayilanlari.
#
# Varsayilanlar dosya hic yoksa da calisabilir bir durum uretiyor:
# anahtar bos oldugu icin AI kapali kalir ve kabuk duz kabuk olarak
# calisir. Anahtarsiz durum bir hata hali degil, desteklenen bir
# calisma bicimi.
DEFAULTS = {
    "provider": "groq",
    "base_url": "https://api.groq.com/openai/v1",
    "api_key": "",
    "model_intent": "",
    "model_explain": "",
    "use_local": "false",
    "local_url": "http://127.0.0.1:11434/v1",
    "local_model": "",
    "local_fallback": "true",
    "timeout": "15",
    "spinner": "4",
}


def default_path():
    """Yapilandirma dosyasinin yolu; NAX_CONF ile degistirilebilir."""
    override = os.environ.get("NAX_CONF")
    if override:
        return override
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(root, "nax.conf")


def parse_text(text, warn=None):
    """Metni anahtar/deger eslemesine cevirir; bilinmeyeni bildirir."""
    values = {}
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            if warn:
                warn("satir %d: esittir isareti yok: %r" % (number, line))
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key not in DEFAULTS:
            if warn:
                warn("satir %d: taninmayan anahtar: %r" % (number, key))
            continue
        values[key] = value
    return values


class Config:
    """nax.conf'un bellekteki hali; degisiklikte kendini yeniler."""

    def __init__(self, path=None):
        self.path = path or default_path()
        self.values = dict(DEFAULTS)
        self.stamp = None
        self.refresh()

    def _warn(self, message):
        """Uyariyi hata cikisina yazar; oradan gunluk dosyasina gider."""
        sys.stderr.write("naxd: %s: %s\n" % (self.path, message))
        sys.stderr.flush()

    def _stamp(self):
        """Dosyanin degisme zamani ve boyutu; yoksa None."""
        try:
            info = os.stat(self.path)
        except OSError:
            return None
        return (info.st_mtime_ns, info.st_size)

    def refresh(self):
        """Dosya degistiyse yeniden okur; okunduysa True doner."""
        stamp = self._stamp()
        if stamp == self.stamp:
            return False
        self.stamp = stamp
        self.values = dict(DEFAULTS)
        if stamp is None:
            return True
        try:
            with open(self.path, encoding="utf-8") as handle:
                text = handle.read()
        except OSError as exc:
            self._warn("okunamadi: %s" % exc)
            return True
        self.values.update(parse_text(text, self._warn))
        return True

    def text(self, key):
        """Alanin metin degerini verir."""
        return self.values.get(key, DEFAULTS.get(key, ""))

    def flag(self, key):
        """Alanin dogru/yanlis degerini verir; taninmayan metin yanlistir."""
        return self.text(key).strip().lower() in ("true", "1", "yes", "on")

    def number(self, key):
        """Alanin sayi degerini verir; bozuk metinde varsayilana doner."""
        try:
            return float(self.text(key))
        except ValueError:
            self._warn("%s sayi degil: %r" % (key, self.text(key)))
            return float(DEFAULTS[key])

    def has_key(self):
        """Bulut anahtari var mi; yoksa AI kapali kalir."""
        return bool(self.text("api_key").strip())
