"""
stub_provider.py - sahte model servisi.

NEDEN GERCEK SERVISE CIKMIYORUZ:
    Testler ag, anahtar ve ucret gerektirmemeli; ustelik gercek bir model
    ayni girdiye her zaman ayni cevabi vermez. Olculmek istenen sey
    modelin kalitesi degil, BIZIM yanit isleme yolumuz: baslik
    ayristirma, hata hali, dusme kurali.

IKI BICIM BIR ARADA:
    Uc nokta "/chat/completions" ise uyumlu bicim, "/messages" ise Claude
    bicimi. Ikisini ayni sunucuda tutmak, testin ayni kiple iki
    bagdastiriciyi de sinamasini sagliyor.

KIP MODEL ADINDAN OKUNUR:
    Istegin "model" alani kip olarak kullaniliyor. Bu sayede yeni bir
    yapilandirma alani eklemek gerekmiyor ve her vaka yalnizca model adini
    degistirerek farkli bir davranis seciyor.
"""

import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# Kip -> modelin dondurecegi metin.
REPLIES = {
    "ok": "CMD ls -la",
    "answer": "ANS bu bir dizin listesi",
    "fenced": "CMD ```sh\nls -la\n```",
    "noprefix": "ls -la",
    "risky": "CMD rm -rf /tmp/kazadan-sonra",
    "explain": "Komut bulunamadi, cunku PATH icinde boyle bir program yok.",
    "local": "CMD yerelden gelen komut",
    "spaced": "  CMD   ls -la  ",
}

# Kip -> (durum kodu, govde). Hata hallerini uretir.
#
# "cf1010" GERCEK BIR VAKANIN KOPYASI: araya giren bir katman (Cloudflare)
# istegi API'ye hic ulastirmadan 403 ile geri cevirdi ve govde JSON bile
# degildi, duz metin "error code: 1010" idi. Govdeyi JSON sanan bir okuyucu
# bu sebebi kaybeder.
FAILURES = {
    "http500": (500, '{"error":"ic hata"}'),
    "http429": (429, '{"error":"cok fazla istek"}'),
    "badjson": (200, "bu JSON degil"),
    "cf1010": (403, "error code: 1010"),
    "nochoices": (200, '{"id":"x"}'),
    "empty": (200, '{"choices":[{"message":{"content":"   "}}]}'),
}


class Handler(BaseHTTPRequestHandler):
    """Tek uc: /v1/chat/completions."""

    def log_message(self, fmt, *args):
        """Sunucunun kendi gunlugu kapali; test ciktisini kirletmesin."""

    def anthropic_body(self, mode, text):
        """Claude biciminde yanit govdesi uretir."""
        if mode == "refused":
            return json.dumps({"stop_reason": "refusal",
                               "stop_details": {"type": "refusal"},
                               "content": []})
        return json.dumps({"stop_reason": "end_turn",
                           "content": [{"type": "text", "text": text}]})

    def do_POST(self):
        """Istegi okur, model adini kip sayar ve ona gore cevap verir."""
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)
        try:
            payload = json.loads(raw.decode("utf-8"))
        except ValueError:
            payload = {}
        mode = str(payload.get("model", "ok"))
        self.server.seen.append({
            "mode": mode,
            "path": self.path,
            "auth": self.headers.get("Authorization", ""),
            "agent": self.headers.get("User-Agent", ""),
            "api_key": self.headers.get("x-api-key", ""),
            "version": self.headers.get("anthropic-version", ""),
            "messages": payload.get("messages", []),
            "system": payload.get("system"),
            "max_tokens": payload.get("max_tokens"),
            "temperature": payload.get("temperature"),
        })
        if mode in FAILURES:
            code, body = FAILURES[mode]
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body.encode("utf-8"))
            return
        if mode == "echo":
            user = ""
            for message in payload.get("messages", []):
                if message.get("role") == "user":
                    user = message.get("content", "")
            text = "CMD echo %s" % user.splitlines()[0]
        else:
            text = REPLIES.get(mode, "CMD ls -la")
        if self.path.endswith("/messages"):
            body = self.anthropic_body(mode, text)
        else:
            body = json.dumps({"choices": [{"message": {"content": text}}]})
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body.encode("utf-8"))


def start():
    """Sunucuyu rastgele bir kapida baslatir; (sunucu, adres) dondurur."""
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.seen = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, "http://127.0.0.1:%d/v1" % server.server_address[1]
