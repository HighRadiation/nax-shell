# nax

AI'ı terminalin *yanına* değil, kabuğun *dilinin içine* koyan bir komut kabuğu.

Ayrı bir mod yok, özel bir işaret yok, çağrılacak bir program adı yok. Aynı
satıra hem komut hem niyet yazılır; hangisi olduğuna kabuk karar verir.

```
nax ~/projeler $ ls -la
...                                  ← normal komut: sıfır gecikme, AI'a hiç uğramaz

nax ~/projeler $ dün değişen dosyaları zip'le
  ↳ find . -newermt '1 day ago' -type f -print0 | xargs -0 zip degisen.zip
nax ~/projeler $ find . -newermt '1 day ago' ...
                                     ← öneri düzenleme satırına yazılır, Enter çalıştırır

nax ~/projeler $ bu dizin ne işe yarıyor
Kabuğun kaynak kodu burada; src/ altında okuma döngüsü ve satır yönetimi var.
                                     ← soru: metin cevap, çalıştırılacak bir şey yok
```

## Neden bu, "terminalde AI çalıştırmak"tan farklı

Ayrı bir AI programına "şu hatayı çöz" dediğinde o program dosya sormak, `ls`
çekmek, tahmin etmek zorundadır. Kabuğun içindeki AI ise çalışma dizinini, son
çalıştırdığın komutları, çıkış kodlarını ve git dalını **hiç sormadan** bilir.

Fark kodun nerede durduğu değil, paylaşılan durum.

## İki kural

**Hiçbir komut sen görmeden çalışmaz.** AI'ın ürettiği satır düzenleme
tamponuna yazılır; çalıştıran şey senin Enter'a basmandır. Yıkıcı komutlarda
ayrıca açık onay istenir.

**AI çökerse kabuk çalışır.** Ağ giderse, anahtar yoksa, yardımcı süreç
ölürse kabuk tek satır uyarı basar ve düz bir kabuk olarak tam işlevle devam
eder. Bu bir kabuk; AI onun bir aşaması.

## Kurulum

Gereken tek harici bağımlılık readline geliştirme başlıkları:

```
sudo apt-get install -y libreadline-dev
make
./nax
```

AI tarafı için yapılandırma:

```
cp nax.conf.example nax.conf
chmod 600 nax.conf          # api_key satirini doldur
```

`nax.conf` olmadan da kabuk tam çalışır; yalnızca AI yolu kapalı kalır.

## Geliştirme

```
make            kabugu derler, uyarilar hata sayilir
make asan       adres ve tanimsiz davranis denetleyicili ikili
make test       testleri normal ikili ile kosar
make check      testleri her iki ikili ile kosar
make clean      uretilen her seyi siler
```

Bu ortamda `valgrind` yok; bellek doğrulaması `make check` üzerinden
denetleyicilerle yapılır. Sızıntı sıfır olmadan hiçbir aşama bitmiş sayılmaz.

## Durum

Çekirdek kabuk kuruluyor. Şu an çalışan: okuma döngüsü, renkli prompt, kalıcı
geçmiş, `exit`, Ctrl-D ve sinyaller — Ctrl-C yarım satırı atıp temiz bir prompt
verir, Ctrl-\ yok sayılır. **Sözcük ayırıcı tamam**: tırnaklar, kaçış
karakteri, operatörler ve kapanmamış tırnak hataları, 44 vakalık bir tabloyla
doğrulanmış halde.

Sıradaki: ayrıştırıcı, genişletme, çalıştırıcı. AI hattı onların üstüne
geliyor.

Bilinen eksikler [docs/FINDINGS.md](docs/FINDINGS.md) içinde kayıtlı.

## Dokümanlar

| Dosya | İçerik |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Boru hattı, süreç modeli, bellek sahipliği |
| [docs/CLASSIFIER.md](docs/CLASSIFIER.md) | "Komut mu niyet mi" kararı, vetolar, tuzaklar |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | Kabuk ile yardımcı süreç arasındaki protokol |
| [docs/CONFIG.md](docs/CONFIG.md) | Sağlayıcılar, yerel model, düşme kuralı |
| [docs/PRIVACY.md](docs/PRIVACY.md) | Neyin gönderildiği ve neyin gönderilmediği |
| [docs/NORMS.md](docs/NORMS.md) | Kod stili sözleşmesi |
| [docs/FINDINGS.md](docs/FINDINGS.md) | Ertelenmiş işler |
