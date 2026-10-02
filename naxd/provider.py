"""
provider.py - bulut ve yerel model cagrilari.

TEK ADAPTOR, COK SAGLAYICI:
    Groq, OpenRouter, OpenAI, NVIDIA ve yerel sunucularin cogu ayni
    "/v1/chat/completions" bicimini konusuyor. Bu yuzden aralarinda gecis
    yapmak yalnizca adres, anahtar ve model adi degistirmek demek - kod
    degismiyor. Claude'un ayri bicimi icin ayri bir adaptor gerekecek ve
    bu modul o yuzden tek bir islev degil.

DUSME TETIKLEYICISI "INTERNET VAR MI" DEGIL:
    Gercek hayatta internetin gitmesi yilda birkac kez olur; servis
    arizasi, istek sinirina takilma, kota bitmesi ve agin yavaslamasi ayda
    birkac kez olur. Mekanizma ayni oldugu icin dogru tetikleyiciyi
    secmek bedava bir kazanc: UST USTE IKI BASARISIZ CAGRI yerel modele
    gecirir.

    Sayac yalnizca bulut cagrilarini sayar ve basarili bir cagri onu
    sifirlar. Yerel cagri basarisiz olursa bu sayaca girmez; yoksa bir kez
    yerele dusen oturum sonsuza kadar orada kalirdi.

HATA METNI KISA TUTULUR:
    Kullanici tek satir gorur. Yigit izi ya da sunucunun tam yaniti
    gunluk dosyasina gider, ekrana degil.
"""

import json
import sys
import urllib.error
import urllib.request

CLOUD_FAIL_LIMIT = 2


class ProviderError(Exception):
    """Cagri basarisiz oldu; mesaji kullaniciya gosterilebilir kisalikta."""


def post_json(url, headers, payload, timeout):
    """JSON gonderip JSON alir; hatalari ProviderError'a cevirir."""
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(url, data=data, method="POST")
    request.add_header("Content-Type", "application/json")
    for key, value in headers.items():
        request.add_header(key, value)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read()
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")[:400]
        sys.stderr.write("naxd: HTTP %s: %s\n" % (exc.code, detail))
        sys.stderr.flush()
        raise ProviderError("servis %s dondurdu" % exc.code) from exc
    except Exception as exc:
        sys.stderr.write("naxd: baglanti hatasi: %r\n" % (exc,))
        sys.stderr.flush()
        raise ProviderError("servise ulasilamadi") from exc
    try:
        return json.loads(body.decode("utf-8", "replace"))
    except ValueError as exc:
        sys.stderr.write("naxd: cozulemeyen yanit: %s\n"
                         % body[:400].decode("utf-8", "replace"))
        sys.stderr.flush()
        raise ProviderError("yanit cozulemedi") from exc


def read_content(data):
    """Yanittaki metni cikarir; beklenen yapi yoksa ProviderError."""
    try:
        content = data["choices"][0]["message"]["content"]
    except (KeyError, IndexError, TypeError) as exc:
        sys.stderr.write("naxd: beklenmeyen yanit yapisi: %r\n" % (data,))
        sys.stderr.flush()
        raise ProviderError("yanit beklenen bicimde degil") from exc
    if not isinstance(content, str) or not content.strip():
        raise ProviderError("yanit bos")
    return content


class Provider:
    """Cagrilari yonetir ve dusme kuralini uygular."""

    def __init__(self, config):
        self.config = config
        self.cloud_fails = 0

    def local_ready(self):
        """Yerel model kullanilabilir mi."""
        return bool(self.config.text("local_url").strip()
                    and self.config.text("local_model").strip())

    def pick_target(self):
        """
        Bu cagri nereye gidecek: ("local" | "cloud", adres, model, anahtar).

        Sira: elle secilen yerel, sonra dusme kurali, sonra bulut.
        """
        if self.config.flag("use_local"):
            if not self.local_ready():
                raise ProviderError("yerel model ayarlanmamis")
            return ("local", self.config.text("local_url"), "", "")
        if (self.config.flag("local_fallback")
                and self.cloud_fails >= CLOUD_FAIL_LIMIT
                and self.local_ready()):
            return ("local", self.config.text("local_url"), "", "")
        if not self.config.has_key():
            raise ProviderError("api_key bos")
        return ("cloud", self.config.text("base_url"), "",
                self.config.text("api_key"))

    def model_for(self, where, task):
        """Hedef ve goreve gore model adini verir."""
        if where == "local":
            return self.config.text("local_model")
        if task == "explain":
            return self.config.text("model_explain")
        return self.config.text("model_intent")

    def ask(self, task, messages):
        """
        Modele sorar ve metni dondurur; basarisizlikta ProviderError.

        task "intent" ya da "explain". Niyet isteginde sicaklik sifir:
        ayni cumlenin ayni komuta cevrilmesi, yaraticiliktan daha
        degerli.
        """
        where, base, _unused, key = self.pick_target()
        model = self.model_for(where, task)
        if not model.strip():
            raise ProviderError("model adi ayarlanmamis")
        headers = {}
        if key:
            headers["Authorization"] = "Bearer %s" % key
        payload = {
            "model": model,
            "messages": messages,
            "temperature": 0.0 if task == "intent" else 0.3,
            "max_tokens": 200 if task == "intent" else 400,
        }
        url = base.rstrip("/") + "/chat/completions"
        try:
            data = post_json(url, headers, payload,
                             self.config.number("timeout"))
        except ProviderError:
            if where == "cloud":
                self.cloud_fails += 1
            raise
        if where == "cloud":
            self.cloud_fails = 0
        return read_content(data)
