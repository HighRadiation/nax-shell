"""
stub_provider.py - sahte model servisi.

NEDEN GERCEK SERVISE CIKMIYORUZ:
    Testler ag, anahtar ve ucret gerektirmemeli; ustelik gercek bir model
    ayni girdiye her zaman ayni cevabi vermez. Olculmek istenen sey
    modelin kalitesi degil, BIZIM yanit isleme yolumuz: baslik
    ayristirma, hata hali, dusme kurali.

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
FAILURES = {
    "http500": (500, '{"error":"ic hata"}'),
    "http429": (429, '{"error":"cok fazla istek"}'),
    "badjson": (200, "bu JSON degil"),
    "nochoices": (200, '{"id":"x"}'),
    "empty": (200, '{"choices":[{"message":{"content":"   "}}]}'),
}


class Handler(BaseHTTPRequestHandler):
    """Tek uc: /v1/chat/completions."""

    def log_message(self, fmt, *args):
        """Sunucunun kendi gunlugu kapali; test ciktisini kirletmesin."""

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
            "auth": self.headers.get("Authorization", ""),
            "messages": payload.get("messages", []),
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
