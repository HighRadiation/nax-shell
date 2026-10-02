"""
provider.py - bulut ve yerel model cagrilari.

NEDEN RESMI SDK DEGIL HAM HTTP:
    Projenin kurali "yalnizca Python standart kutuphanesi" - kullanicinin
    paket kurmasi gerekmemeli. Resmi istemci kutuphaneleri bu kurali
    bozardi, o yuzden istekler urllib ile elden kuruluyor. Bu bilincli bir
    secim, kolaylik degil.

IKI ADAPTOR, COK SAGLAYICI:
    Groq, OpenRouter, OpenAI, NVIDIA ve yerel sunucularin cogu ayni
    "/v1/chat/completions" bicimini konusuyor. Bu yuzden aralarinda gecis
    yapmak yalnizca adres, anahtar ve model adi degistirmek demek.

    Claude ayri bir bicim konusuyor ve farklari KUCUK DEGIL:
      - uc nokta "/messages", baslik "x-api-key" ve "anthropic-version"
      - sistem istemi bir MESAJ degil, ust duzey "system" alani
      - yanit metni "content" listesindeki "text" turu bloklarda
      - "max_tokens" zorunlu
      - SICAKLIK GONDERILMEZ: guncel modellerde "temperature" reddediliyor
        ve istek 400 ile donuyor. Uyumlu adaptor sicaklik gonderdigi icin
        ayni govdeyi iki yere yollamak mumkun degil.

    Yerel sunucular uyumlu bicimi konustugu icin dusme yolu her zaman
    uyumlu adaptoru kullaniyor.

DUSME TETIKLEYICISI "INTERNET VAR MI" DEGIL:
    Gercek hayatta internetin gitmesi yilda birkac kez olur; servis
    arizasi, istek sinirina takilma, kota bitmesi ve agin yavaslamasi ayda
    birkac kez olur. Mekanizma ayni oldugu icin dogru tetikleyiciyi
    secmek bedava bir kazanc: UST USTE IKI BASARISIZ CAGRI yerel modele
    gecirir.

    Sayac yalnizca bulut cagrilarini sayar ve basarili bir cagri onu
    sifirlar. Yerel cagri basarisiz olursa bu sayaca girmez; yoksa bir kez
    yerele dusen oturum sonsuza kadar orada kalirdi.

HATA METNI KISA TUTULUR AMA SEBEBI TASIR:
    Kullanici tek satir gorur ve o satirda saglayicinin kendi gerekcesi
    yazar. Yalnizca durum kodu gostermek ("servis 403 dondurdu") kullaniciyi
    yanlis yere bakmaya itiyor: 403 gorunce insan once anahtarini sucluyor,
    oysa sebep bambaska olabiliyor. Yigit izi ve sunucunun TAM yaniti
    gunluk dosyasina gider, ekrana degil.
"""

import json
import sys
import urllib.error
import urllib.request

CLOUD_FAIL_LIMIT = 2

# Claude bicimi icin zorunlu surum basligi.
ANTHROPIC_VERSION = "2023-06-01"

# Istek imzasi.
#
# NEDEN VARSAYILANI BIRAKMIYORUZ: buyuk saglayicilarin onunde Cloudflare
# duruyor ve urllib'in varsayilan "Python-urllib/3.x" imzasi bot listesinde.
# Istek API'ye HIC ULASMADAN "HTTP 403 - error code: 1010" ile geri donuyor;
# anahtar dogru olsa bile. 1010 Cloudflare'in "bu imza yasakli" kodu, yani
# bir yetki hatasi degil. Kendi adimizi yazmak istegi o listeden cikariyor.
USER_AGENT = "nax/1.0"


class ProviderError(Exception):
    """Cagri basarisiz oldu; mesaji kullaniciya gosterilebilir kisalikta."""


def error_reason(body):
    """
    Saglayicinin hata govdesinden tek satirlik sebebi cikarir.

    Govde cogunlukla {"error": {"message": ...}} bicimindedir ama her zaman
    degil: Cloudflare gibi araya giren katmanlar duz metin dondurur. Bu
    yuzden cozulemeyen govde ATILMAZ, kirpilip oldugu gibi kullanilir -
    "error code: 1010" tam olarak boyle bir metin ve tesadufen en ogretici
    olani.
    """
    text = " ".join((body or "").split())
    try:
        data = json.loads(text)
    except ValueError:
        data = None
    if isinstance(data, dict):
        inner = data.get("error")
        if isinstance(inner, dict):
            text = str(inner.get("message") or inner.get("type") or text)
        elif isinstance(inner, str):
            text = inner
        elif data.get("message"):
            text = str(data["message"])
    text = " ".join(text.split())
    if len(text) > 80:
        text = text[:77] + "..."
    return text


def post_json(url, headers, payload, timeout):
    """JSON gonderip JSON alir; hatalari ProviderError'a cevirir."""
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(url, data=data, method="POST")
    request.add_header("Content-Type", "application/json")
    request.add_header("User-Agent", USER_AGENT)
    for key, value in headers.items():
        request.add_header(key, value)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read()
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")[:400]
        sys.stderr.write("naxd: HTTP %s: %s\n" % (exc.code, detail))
        sys.stderr.flush()
        reason = error_reason(detail)
        if reason:
            raise ProviderError("servis %s dondurdu: %s"
                                % (exc.code, reason)) from exc
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


def empty_reason(data):
    """
    Bos yanitin sebebini kisa bir ifadeyle adlandirir.

    "finish_reason" bos yanitin tek ipucu: "length" cevabin KESILDIGINI
    soyluyor, yani model konusmaya basladi ama jeton siniri doldu. Bu,
    akil yurutme yapan modellerde sik gorulur - butce akil yurutmeye
    gidiyor ve kullaniciya gosterilecek metin hic uretilmiyor. Sebebi
    adlandirmamak kullaniciyi yanlis yere, "model sacmaladi" sanisina
    goturuyor.
    """
    try:
        choice = data["choices"][0]
    except (KeyError, IndexError, TypeError):
        return ""
    stop = choice.get("finish_reason") or ""
    if stop == "length":
        return "jeton siniri doldu, cevap kesildi"
    if stop:
        return "bitis sebebi: %s" % stop
    return ""


def read_content(data):
    """Yanittaki metni cikarir; beklenen yapi yoksa ProviderError."""
    try:
        content = data["choices"][0]["message"]["content"]
    except (KeyError, IndexError, TypeError) as exc:
        sys.stderr.write("naxd: beklenmeyen yanit yapisi: %r\n" % (data,))
        sys.stderr.flush()
        raise ProviderError("yanit beklenen bicimde degil") from exc
    if not isinstance(content, str) or not content.strip():
        sys.stderr.write("naxd: bos yanit govdesi: %r\n" % (repr(data)[:600],))
        sys.stderr.flush()
        reason = empty_reason(data)
        if reason:
            raise ProviderError("yanit bos (%s)" % reason)
        raise ProviderError("yanit bos")
    return content


def split_system(messages):
    """Sistem istemini mesajlardan ayirir; (metin, kalan) dondurur."""
    system = []
    rest = []
    for message in messages:
        if message.get("role") == "system":
            system.append(message.get("content", ""))
        else:
            rest.append(message)
    return ("\n\n".join(system), rest)


def build_compat(base, key, model, messages, limit, task, effort=""):
    """
    Uyumlu bicimde istek kurar; (adres, baslik, govde) dondurur.

    "reasoning_effort" YALNIZCA DOLUYSA gonderiliyor. Akil yurutme yapan
    modellerde butcenin tamami goze gorunmeyen akil yurutme asamasina
    gidebiliyor ve kullaniciya bos cevap kaliyor; bu alan o asamayi
    kisaltiyor. Ama alani taniyan her saglayici yok ve bilinmeyen alan
    gonderen bir istek 400 ile donebilir - o yuzden varsayilan bos ve
    karar yapilandirmaya birakiliyor.
    """
    headers = {}
    if key:
        headers["Authorization"] = "Bearer %s" % key
    payload = {
        "model": model,
        "messages": messages,
        "temperature": 0.0 if task == "intent" else 0.3,
        "max_tokens": limit,
    }
    if effort.strip():
        payload["reasoning_effort"] = effort.strip()
    return (base.rstrip("/") + "/chat/completions", headers, payload)


def build_anthropic(base, key, model, messages, limit):
    """
    Claude biciminde istek kurar; (adres, baslik, govde) dondurur.

    UC FARK, hepsi zorunlu:
      - sistem istemi ust duzey "system" alaninda, mesaj listesinde DEGIL
      - anahtar "x-api-key" basliginda ve surum basligi sart
      - SICAKLIK YOK: guncel modellerde gonderilmesi istegi 400 yapiyor
    """
    system, rest = split_system(messages)
    headers = {"anthropic-version": ANTHROPIC_VERSION}
    if key:
        headers["x-api-key"] = key
    payload = {
        "model": model,
        "max_tokens": limit,
        "messages": rest,
    }
    if system:
        payload["system"] = system
    return (base.rstrip("/") + "/messages", headers, payload)


def read_anthropic(data):
    """
    Claude yanitindaki metni cikarir; beklenen yapi yoksa ProviderError.

    Yanit metni "content" listesindeki "text" turu bloklarda duruyor ve
    birden fazla blok olabilir. Ayrica istek guvenlik nedeniyle
    reddedilmis olabilir: bu HTTP 200 ile geliyor, yani durum kodu
    yetmiyor ve "stop_reason" denetlenmek zorunda.
    """
    if isinstance(data, dict) and data.get("stop_reason") == "refusal":
        raise ProviderError("istek reddedildi")
    blocks = data.get("content") if isinstance(data, dict) else None
    if not isinstance(blocks, list):
        sys.stderr.write("naxd: beklenmeyen yanit yapisi: %r\n" % (data,))
        sys.stderr.flush()
        raise ProviderError("yanit beklenen bicimde degil")
    parts = []
    for block in blocks:
        if isinstance(block, dict) and block.get("type") == "text":
            parts.append(block.get("text", ""))
    text = "".join(parts)
    if not text.strip():
        raise ProviderError("yanit bos")
    return text


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

    def uses_anthropic(self, where):
        """Bu cagri Claude bicimini mi kullanacak."""
        if where == "local":
            return False
        return self.config.text("provider").strip().lower() == "anthropic"

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
        limit = 512 if task == "intent" else 800
        if self.uses_anthropic(where):
            url, headers, payload = build_anthropic(base, key, model,
                                                    messages, limit)
            reader = read_anthropic
        else:
            effort = ""
            if where == "cloud":
                effort = self.config.text("reasoning_effort")
            url, headers, payload = build_compat(base, key, model, messages,
                                                 limit, task, effort)
            reader = read_content
        try:
            data = post_json(url, headers, payload,
                             self.config.number("timeout"))
        except ProviderError:
            if where == "cloud":
                self.cloud_fails += 1
            raise
        if where == "cloud":
            self.cloud_fails = 0
        return reader(data)
