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

## Dilbilgisi

Şu an ayrıştırılan dil:

```
boru_hattı  : komut ( '|' komut )*
komut       : ( sözcük | yönlendirme )+
yönlendirme : ( '<' | '>' | '>>' | '<<' ) sözcük
```

Sözcük ayırıcı `;`, `&&`, `||`, `&`, `(`, `)` operatörlerini de tanıyor ama
ayrıştırıcı henüz kabul etmiyor; anlaşılır bir hata verir. Onları şimdi
tanımak, sözcük ayırıcıyı ileride ikinci kez açmayı önlüyor.

Bir komut ya en az bir argüman ya da en az bir yönlendirme içermek zorunda.
Argümansız ama yönlendirmeli komut **geçerlidir**: `> dosya`, dosyayı
oluşturup hiçbir şey çalıştırmamak demektir. İkisi de boş olan komut ise
sözdizimi hatasıdır — `ls | | wc` böyle yakalanır.

Yönlendirme komutun başında, ortasında ya da sonunda olabilir; `> out echo a`
ile `echo a > out` aynı ağacı üretir. Argümanların ve yönlendirmelerin kendi
içindeki **sırası korunur**, çünkü `> a > b` ile `> b > a` aynı şey değildir.

## Tırnak kipi: lexer ile genişletme arasındaki sözleşme

Sözcük ayırıcı tırnakları **silmez**, nerede başladıklarını kaydeder. Bir
sözcük, her biri tek bir tırnak kipine sahip **parçaların** zinciri olur:

| Kip | Anlamı |
|---|---|
| `none` | genişlet ve alan ayırmaya tabi tut |
| `dq` | genişlet ama alan ayırma |
| `sq` | hiçbir şey yapma, harfi harfine al |

Kaçış karakteri bir karakteri `sq` kipine **düşürür**. Böylece `\$HOME` ile
`'$HOME'` aynı sonucu verir ve genişletme aşaması kaçış kontrolü yapmak zorunda
kalmaz. Aynı mantıkla çift tırnak içindeki `\$` o karakteri kendi parçasına
ayırıp `sq` yapar.

Bu modelin kazancı: tırnak bilgisi **tek bir yerde** üretilir ve genişletme
aşaması tırnakları yeniden ayrıştırmak zorunda kalmaz; ne yapacağını parçanın
kipinden okur. Boş dize sorununu da çözer — `""` gerçek bir boş argümandır
(tek parça, `dq`, boş metin), ama genişlemeden gelen boş değer argüman
üretmez.

## Genişletme

Sıra (basitleştirilmiş POSIX sırası):

1. `~` genişletmesi — yalnızca sözcüğün **ilk** tırnaksız parçasında
2. `$` genişletmesi — `$NAME`, `${NAME}`, `$?`
3. Alan ayırma — **yalnızca** tırnaksız genişletmeden gelen metinde
4. Tırnak kaldırma — sözcük ayırıcı zaten yaptı, burada iş kalmadı

Dördüncü adımın boş olması tırnak kipi sözleşmesinin karşılığı: tırnaklar
lexer'da silindi, yerine parça kipi kaldı. Genişletme tırnak ayrıştırmıyor,
ne yapacağını parçanın kipinden okuyor.

**Alan ayırmanın ince noktası.** Harfi harfine yazılmış metin yeniden
bölünmez — sözcük ayırıcı onu zaten boşluklardan bölmüştü. Yalnızca
genişletmeden *gelen* metin bölünür:

```
X="a b"
echo $X      → iki alan
echo "$X"    → tek alan
```

**Boş alan kuralı.** Bir bayrak (`emit`) sözcük başına değil **alan başına**
tutulur ve her alan üretildiğinde sıfırlanır. Bayrağı kuran şey harfi harfine
ya da tırnaklı içerik; genişletmeden gelen karakterler kurmaz:

| Girdi | Alan sayısı |
|---|---|
| `echo $YOK` | 0 — sözcük kaybolur |
| `echo "$YOK"` | 1, boş |
| `TSP="a "; echo $TSP""` | 2 — `a` ve boş olan |

Üçü de bash ölçülerek doğrulandı. Bayrak sözcük başına tutulduğunda üçüncüsü
kayboluyordu; bu hatayı mutasyon denemesi yakaladı.

**Belirsiz yönlendirme.** Yönlendirme hedefi tam olarak bir alan üretmek
zorunda. `> $YOK` neyi yönlendireceğini, `> $IKI_KELIME` hangisine
yönlendireceğini söylemiyor; ikisi de hata.

**Değişken adı doğrulanır.** `${...}` içindeki ad geçerli bir isim değilse
hata verilir. Doğrulama olmadan `${VAR:-varsayılan}` gibi desteklenmeyen bir
biçim `VAR:-varsayılan` adını arar, bulamaz ve **sessizce boşa genişler** —
sessiz yanlış cevap hatadan kötüdür.

## Bellek sahipliği

Sızıntı disiplininin tek kuralı: **her modül kendi ürettiği tipi kendisi
serbest bırakır.**

| Tip | Üreten | Serbest bırakan |
|---|---|---|
| girdi satırı | `line.c` (`ln_read`) | çağıran (`main.c` döngüsü) |
| geçmiş yolu | `line.c` (`ln_hist_path`) | `main.c` (`shell_free`) |
| prompt metni | `line.c` (`build_prompt`) | `line.c`, okuma biter biter |
| token listesi | `lexer.c` (`lex_split`) | çağıran (`lex_free`) |
| sözcük parçaları | `lexer.c` | `lex_free`, token ile birlikte |
| tampon metni | `buf.c` (`buf_take`) | çağıran; devredilmeyen tampon `buf_free` |
| kanonik metin | `lex_dump.c`, `ast_dump.c` | çağıran |
| boru hattı ağacı | `parser.c` (`ast_build`) | çağıran (`ast_free`) |
| alan listesi | `expand.c` (`exp_word`) | çağıran (`field_free`) |
| genişletilmiş komut | `expand_cmd.c` (`exp_cmd`) | çağıran (`xcmd_free`) |

**Ağaç token'ları ödünç alır.** Ağaç düğümleri yalnızca kendi struct'larının
sahibidir; içerdikleri sözcük token'ları sözcük ayırıcının listesinde kalır.
Bunun iki sonucu var:

1. Çağıran **ikisini de** bırakmak zorunda: `ast_free` ve `lex_free`.
2. Token listesi ağaçtan önce bırakılamaz.

Alternatifi ağacın token listesini devralmasıydı. O durumda ayrıştırma yarıda
hata verdiğinde token'ların bir kısmı ağaçta, bir kısmı listede kalır ve kimin
neyi bırakacağı her hata yolunda yeniden düşünülmek zorunda kalırdı. Ödünç
alma bu soruyu tamamen ortadan kaldırıyor — sıra bile önemli değil.

Yeni bir tip eklendiğinde bu tabloya bir satır eklenir ve serbest bırakma
fonksiyonu tipin yaşadığı modülde durur (`tok_free` sözcük ayırıcıda,
`ast_free` ayrıştırıcıda). Doğrulama `make check` ile yapılır; ayrıntısı
[NORMS.md](NORMS.md) içinde.

## Dosya düzeni

```
src/
  nax.h          ortak tipler ve paylaşılan bildirimler
  parse.h        dil tarafının tipleri: tırnak kipi, parça, token
  main.c         giriş noktası, okuma döngüsü, kurulum ve kapanış
  line.c         satır okuma, prompt, geçmiş
  signal.c       etkileşimli sinyal davranışı (Ctrl-C, Ctrl-\)
  buf.c          büyüyebilen metin tamponu (üç modül birlikte kullanır)
  lexer.c        satırı token listesine çevirir
  lex_dump.c     token listesini kanonik metne çevirir (test ve ayıklama)
  parser.c       token listesini boru hattı ağacına çevirir
  ast_dump.c     ağacı kanonik metne çevirir (test ve ayıklama)
  expand.c       bir sözcüğü alanlara çevirir ($VAR, $?, ~, alan ayırma)
  expand_cmd.c   bir komutun tüm argüman ve yönlendirmelerini genişletir
  exp_dump.c     genişletilmiş hattı kanonik metne çevirir (test ve ayıklama)
test/
  run.sh         tek test giriş noktası; make test ve make check bunu çağırır
  test.h         test koşucularının paylaştığı tipler ve iskele
  harness.c      vaka dosyası okuma, kaçış çözme, özet basma
  test_lexer.c   tablo tabanlı sözcük ayırıcı testleri
  test_parser.c  tablo tabanlı ayrıştırıcı testleri
  test_expand.c  tablo tabanlı genişletme testleri
  cases/*.tsv    vaka tabloları; yeni vaka için yeniden derleme gerekmez
  pty_drive.py   sahte terminal üzerinden etkileşimli yol testleri
naxd/            AI yardımcı süreci (sonraki aşamada)
docs/            bu dizin
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
