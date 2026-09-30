# Mimari

## nax nedir

`nax`, AI'ı terminalin *yanına* değil, kabuğun *dilinin içine* koyan bir komut
kabuğudur. Ayrı bir mod yok, özel bir işaret yok, çağrılacak bir program adı
yok. Kullanıcı `ls -la` da yazabilir, "dün değişen dosyaları göster" de
yazabilir; hangisi olduğuna kabuk kendi karar verir.

Piyasadaki AI destekli terminaller AI'ı bir yan panel ya da ayrı bir komut
olarak tutar. Buradaki fark grameri kendimizin yazması: AI, kabuk hattının bir
aşaması olabiliyor.

İkinci ve daha önemli fark **paylaşılan durum**. Ayrı bir AI programına "şu
hatayı çöz" dediğinde o program dosya sormak, `ls` çekmek, tahmin etmek
zorundadır. Kabuğun içindeki AI ise çalışma dizinini, son çalıştırılan
komutları, çıkış kodlarını ve git dalını hiç sormadan bilir.

## Boru hattı

```
                nax (C)                              naxd (Python)
        ─────────────────────                   ─────────────────────
          satır okuma (line.c)
                   │
          sınıflandırıcı ── niyet ──────────→  istek kaydı (stdin)
                   │ komut                              │
          lexer → parser → expand              yapılandırma: bulut / yerel
                   │                                    │ kalıcı HTTPS
          exec (fork/execve/pipe/dup2)         yanıt kaydı (stdout)
                   │                            ←───────┘
          $? → session (bağlam) ──────────────────┘
```

Klasik kabuk hattı (okuma → sözcüklere ayırma → ayrıştırma → genişletme →
çalıştırma → çıkış kodu) korunur. Sınıflandırıcı bu hattın **başına** eklenen
tek yeni aşamadır ve çoğu satır için hiçbir maliyet getirmez.

## Süreç modeli

Kabuk açılışta `naxd`'yi **bir kez** başlatır, oturum boyunca standart giriş ve
çıkış üzerinden satır tabanlı bir protokolle konuşur, kapanışta öldürür.
Kullanıcı `naxd`'nin varlığını hiç görmez — editörlerin dil sunucusu gibi.

Neden ayrı süreç:

- **Kalıcı bağlantı.** HTTPS el sıkışması oturum başına bir kez ödenir, her
  istekte değil. Bu yaklaşık 100-200 ms tasarruf demek.
- **Konuşma bağlamı yaşar.** Oturum boyunca biriken durum `naxd`'de durur.
- **Hızlı yineleme.** İstem metni ve model seçimi kabuk yeniden derlenmeden
  değişir.
- **Doğru dil doğru işte.** HTTPS, JSON ve istem mühendisliği C'de tatsız,
  Python'da bedava; süreç yönetimi ve terminal denetimi C'de doğal.

`naxd` yalnızca Python standart kütüphanesini kullanır. `pip` ya da sanal ortam
gerekmez; bu, "bağımlılık kurulumu patladı" diye bir hata sınıfını tamamen
ortadan kaldırır.

## AI asla çalıştırmaz

Mimarinin değişmez kuralı: `naxd` hiçbir komut çalıştırmaz ve dosya sistemine
dokunmaz. Yapabildiği tek şey metin döndürmek. Bağlama ihtiyaç duyduğunda
kabuktan **sabit bir listedeki** olguları ister (dizin listesi, git durumu,
bir komutun yolu); kabuk bu isteği karşılar ya da karşılamaz. Komut
çalıştırma yetkisi yalnızca kullanıcıdadır.

Buna bağlı ikinci kural: AI'ın ürettiği hiçbir komut kendiliğinden
çalışmaz. Öneri kullanıcının düzenleme satırına yazılır; çalıştıran şey
kullanıcının Enter'a basmasıdır.

## AI hattı çökerse kabuk çalışmaya devam eder

Bu bir kabuk, bir AI istemcisi değil. `naxd` ölürse, ağ giderse, anahtar
yoksa ya da zaman aşımı olursa kabuk tek satır uyarı basar ve düz bir kabuk
olarak tam işlevle çalışmaya devam eder. Hiçbir AI arızası bir komutu
engellemez.

## Bellek sahipliği

Sızıntı disiplininin tek kuralı: **her modül kendi ürettiği tipi kendisi
serbest bırakır.**

| Tip | Üreten | Serbest bırakan |
|---|---|---|
| girdi satırı | `line.c` (`ln_read`) | çağıran (`main.c` döngüsü) |
| geçmiş yolu | `line.c` (`ln_hist_path`) | `main.c` (`shell_free`) |
| prompt metni | `line.c` (`build_prompt`) | `line.c`, okuma biter biter |

Yeni bir tip eklendiğinde bu tabloya bir satır eklenir ve serbest bırakma
fonksiyonu tipin yaşadığı modülde durur (`tok_free` sözcük ayırıcıda,
`ast_free` ayrıştırıcıda). Doğrulama `make check` ile yapılır; ayrıntısı
[NORMS.md](NORMS.md) içinde.

## Dosya düzeni

```
src/
  nax.h        ortak tipler ve paylaşılan bildirimler
  main.c       giriş noktası, okuma döngüsü, kurulum ve kapanış
  line.c       satır okuma, prompt, geçmiş
  signal.c     etkileşimli sinyal davranışı (Ctrl-C, Ctrl-\)
test/
  run.sh       tek test giriş noktası; make test ve make check bunu çağırır
  pty_drive.py sahte terminal üzerinden etkileşimli yol testleri
naxd/          AI yardımcı süreci (sonraki aşamada)
docs/          bu dizin
```

İki test giriş noktası olmasının sebebi, birinin diğerinin göremediği kodu
kapsaması: boru ile beslenen testler terminal olmadığı için etkileşimsiz yolu
koşar, `readline`, geçmiş ve prompt üretimi orada hiç çalışmaz. Sahte terminal
testleri bu boşluğu kapatır ve denetleyicili ikili ile koşturulduğunda
etkileşimli yolun bellek doğrulamasını da yapar.

Dosya sayısı bilinçli olarak dengede tutulur: ne her şeyi tek dosyaya yığmak,
ne her fonksiyon için ayrı dosya açmak. Aynı gerekçeyle her `.c` için ayrı bir
`.h` yazılmaz; paylaşılan bildirimler `nax.h` içinde, alan bazlı olanlar alan
başlıklarında toplanır.
