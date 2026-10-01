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

## Çalıştırma

Çıkış kodu semantiği (bash ölçülerek doğrulandı):

| Durum | Kod |
|---|---|
| Normal çıkış | çocuğun kendi kodu |
| Sinyalle ölüm | `128 + sinyal` |
| Komut bulunamadı | 127 |
| Çalıştırılamadı (dizin, yetki yok) | 126 |
| Yönlendirme hatası | 1 |
| Boş komut | 0, hiçbir şey çalışmaz |
| **Boru hattı** | **son** komutun kodu |
| `exit` sayısal olmayan argüman | 2, ve kabuk **çıkar** |
| `exit` fazla argüman | 1, ve kabuk **çıkmaz** |

Son satır gerçek bir durum: `$YOKBOYLE` tek başına yazıldığında genişletme
hiç alan üretmez, yani çalıştırılacak bir şey yoktur.

**Komut çözümlemenin iki ayrı kuralı var.** Ad `/` içeriyorsa `PATH`'e hiç
bakılmaz ve başarısızlığın sebebi ayırt edilir (yok / dizin / yetki yok). Ad
`/` içermiyorsa `PATH` taranır ve **yalnızca çalıştırılabilir dosyalar**
sayılır; hiçbiri bulunmazsa 127. Ölçülen sonuç: `PATH` içinde aynı adda
çalıştırılamayan bir dosya bulunsa bile bash 126 değil 127 veriyor.

**`PATH` önbelleği yok.** Çözümleme komut başına bir kez oluyor ve altı dizini
taramak mikrosaniyeler sürüyor. Önbellek sınıflandırıcı için anlamlı olacak —
orada karar milisaniyenin altında kalmak zorunda — ve o zaman eklenecek.

**Boru hattının kodu son komuttan gelir** — `false | true` 0, `true | false`
1 döner. Ama **tüm** çocuklar beklenmek zorunda, yoksa zombi kalır.

**Çözümleme çocukta yapılır.** Her aşama kendi komutunu çözer. Boru hattında
zorunlu; tek komutta da aynı yolu kullanmak kod yolunu tekilleştiriyor.
Gözlenebilir davranış aynı: bulunamayan komut yine 127 döner.

### Boru uçlarının kapatılması

İki ayrı kapatma var ve **ikisi ayrı hata** — ilk yazımda karıştırılmıştı,
mutasyon denemesi ortaya çıkardı.

**Çocukta `spare_fd`:** bir aşama, kendi çıkış borusunun *okuma* ucunu da
devralır. Kapatılmazsa o tanımlayıcı çocuğa, oradan da `execve` ile çalışan
programa **sızar**. Ölçüldü: kapatılmadığında ilk aşama `/proc/self/fd` içinde
`0,1,2` dışında fazladan bir giriş görüyor, bash'te görmüyor. Bu bir asılma
sebebi *değil*.

**Ana süreçte `fds[1]`:** asılmaya yol açan şey budur. Boru EOF'u *yazma*
uçları kapandığında görülür; ana süreç yazma ucunu kapatmazsa yazan taraf hiç
kapanmış sayılmaz ve okuyan aşama sonsuza kadar bekler. Ölçüldü: bu kapatma
kaldırıldığında iki aşamalı bir hat bile asılıyor.

Ölçülen sonuç: 400 boru hattı üst üste koştuktan sonra kabuğun açık
tanımlayıcı sayısı **3** (yalnızca 0, 1, 2) ve hiç zombi yok.

### Yerleşikler nerede koşar

| Durum | Nerede | Neden |
|---|---|---|
| Tek başına yerleşik | **ana süreç** | `cd`, `export`, `unset`, `exit` kabuğun durumunu değiştirir; çocukta koşarsa değişiklik çocukla birlikte yok olur |
| Boru hattı içindeki yerleşik | çocuk | Standart davranış; bash'te de `echo x \| cd /tmp` kabuğun dizinini değiştirmiyor (ölçüldü) |

Ana süreçte koşmanın bedeli var: yönlendirme orada `0` ve `1`'i değiştirdiği
için **önce kaydedilip sonra geri yüklenmek** zorunda. Çocukta bu sorun yoktu
— çocuk zaten yok oluyordu.

Geri yüklemeden **önce `fflush`** zorunlu. Yerleşiğin tamponda bekleyen
çıktısı boşaltılmazsa, tanımlayıcılar geri yüklendikten sonra yanlış yere
yazılır: `pwd > dosya` çıktısını terminale basardı.

Yerleşik olup olmadığına `argv[0]` **genişletildikten sonra** bakılır, böylece
`CMD=cd` iken `$CMD /tmp` de çalışır; bash da böyle davranıyor.

**`env` yerleşik değil.** Plan listesinde vardı ama yerleşik yazmak bizi
bash'ten *uzaklaştırırdı*: `/usr/bin/env` zaten PATH'te ve ortamı birebir aynı
basıyor, çünkü bu kabuk kendi değişken deposunu tutmuyor, doğrudan süreç
ortamını kullanıyor. Yerleşik yapmanın tek sonucu `env a b` gibi
kullanımlarda bash'ten farklı davranmak olurdu.

### Yönlendirmeler borulardan sonra uygulanır

Çakışma halinde yönlendirme kazanmak zorunda: `ls > f | wc` çıktısını dosyaya
yazar, `wc`'ye hiçbir şey gitmez. Sıra bu yüzden önemli, ve bash de böyle
davranıyor (ölçüldü).

Yönlendirmeler **çocukta** uygulanır. Ana süreçte `dup2` yapmak kabuğun kendi
girdi ve çıktısını bozar ve sonra geri yüklemek gerekir; çocukta yapmak o
soruyu tamamen ortadan kaldırıyor.

Yönlendirme listesinin kendi sırası da korunur: `> a > b` iki dosyayı da açar,
çıktıyı `b`'ye bağlar, `a` boş olarak oluşur.

**Çocukta sinyaller sıfırlanır, ve bu zorunlu.** `execve` *yakalanan* sinyalleri
varsayılana döndürür ama *yok sayılan* sinyalleri yok sayılı bırakır. Kabuk
`SIGQUIT`'i yok sayıyor; sıfırlanmazsa her çocuk bunu devralır. Bakım kuralı:
kabuk yeni bir sinyali yok saymaya başlarsa o sinyal `sig_reset_child`'a da
eklenmek zorunda.

**`EINTR` döngüsü zorunlu.** Sinyaller `SA_RESTART` olmadan kuruldu, yani ön
planda bir komut koşarken Ctrl-C `waitpid`'i keser. Sarılmazsa kabuk çocuğu
beklemeyi bırakır, zombi kalır ve çıkış kodu uydurma olur. Aynı şekilde komut
bittikten sonra kesme bayrağı temizlenmek zorunda; temizlenmezse bir sonraki
okumada readline satırı kullanıcı bir şey yazmadan iptal eder.

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
| çözülmüş komut yolu | `path.c` (`path_resolve`) | çağıran |
| `argv` dizisi | `exec.c` (`build_argv`) | çağıran; **metinler kopyalanmaz**, alan listesinden ödünç alınır |

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
  nax.h            ortak tipler; her grup buna bakar
  main.c           giriş noktası, okuma döngüsü, kurulum ve kapanış
  core/            kabuğun altyapısı
    buf.c          büyüyebilen metin tamponu
    line.c         satır okuma, prompt, geçmiş
    signal.c       etkileşimli sinyal davranışı (Ctrl-C, Ctrl-\)
  parse/           dilin tarafı
    parse.h        tırnak kipi, parça, token, ağaç, alan tipleri
    lexer.c        satırı token listesine çevirir
    lex_dump.c     token listesini kanonik metne çevirir
    parser.c       token listesini boru hattı ağacına çevirir
    ast_dump.c     ağacı kanonik metne çevirir
    expand.c       bir sözcüğü alanlara çevirir
    expand_cmd.c   bir komutun argüman ve yönlendirmelerini genişletir
    exp_dump.c     genişletilmiş hattı kanonik metne çevirir
  exec/            çalıştırma
    exec.h         çözümleme sonucu, aşama bağlantıları, bildirimler
    path.c         komut adını çalıştırılabilir bir yola çözer
    exec.c         ANA süreç tarafı: kim çatallanır, kim beklenir
    stage.c        ÇOCUK tarafı: boru uçları, yönlendirme, komut
    redir.c        yönlendirmeleri uygular
    builtin.c      yerleşik tablosu; echo, pwd, exit
    builtin_env.c  export, unset
    builtin_cd.c   cd
  ai/              satır komut mu niyet mi
    ai.h           yol, veto maskesi, karar tipleri
    classifier.c   karar sırası: ön filtre, baş çözümü, yol seçimi
    veto.c         şekil vetoları: baş çözülse bile satır doğal dil mi
    fix.c          yerel yazım düzeltmesi; AI'a gitmeden öneri
    proto.h        tel biçiminin tipleri: kayıt, alan, hata
    b64.c          serbest metin alanları için base64
    proto.c        kayıt kurma ve çözme
    lineio.c       akıştan tam kayıt satırları toplama
  (ai/ içinde sonraki aşamada: yardımcı sürecin başlatılması ve gözetimi)
test/
  test.h           test koşucularının paylaştığı tipler ve iskele
  harness.c        vaka dosyası okuma, kaçış çözme, özet basma
  run.sh           tek test giriş noktası
  test_lexer.c     tablo tabanlı sözcük ayırıcı testleri
  test_parser.c    tablo tabanlı ayrıştırıcı testleri
  test_expand.c    tablo tabanlı genişletme testleri
  test_classify.c  tablo tabanlı sınıflandırıcı testleri; kendi fikstürünü kurar
  test_proto.c     tablo tabanlı protokol testleri; kurma ve çözme bir arada
  test_lineio.c    senaryo tabanlı satır okuma testleri; parçalı veri
  cases/*.tsv      vaka tabloları; yeni vaka için yeniden derleme gerekmez
  pty_drive.py     sahte terminal üzerinden etkileşimli yol testleri
naxd/              AI yardımcı süreci (sonraki aşamada)
docs/              bu dizin
```

Gruplar **mimarinin kendisini** yansıtıyor: altyapı, dil, çalıştırma, ve
sıradaki AI tarafı. On yedi dosya düz durduğunda okuyan haritayı kaybediyordu.

Alan başlıkları kendi klasörlerinde duruyor; `Makefile` her klasörü `-I`
listesine eklediği için dosyalarda `"parse.h"` yazmak yeterli. Gruplama
okunabilirlik için, dahil etme satırlarını uzatmak için değil.

Nesne ağacı kaynak ağacını **aynalar** (`obj/parse/lexer.o`), böylece iki
klasörde aynı adda kaynak olsa bile nesneleri çarpışmaz.

İki test giriş noktası olmasının sebebi, birinin diğerinin göremediği kodu
kapsaması: boru ile beslenen testler terminal olmadığı için etkileşimsiz yolu
koşar, `readline`, geçmiş ve prompt üretimi orada hiç çalışmaz. Sahte terminal
testleri bu boşluğu kapatır ve denetleyicili ikili ile koşturulduğunda
etkileşimli yolun bellek doğrulamasını da yapar.

Dosya sayısı bilinçli olarak dengede tutulur: ne her şeyi tek dosyaya yığmak,
ne her fonksiyon için ayrı dosya açmak. Aynı gerekçeyle her `.c` için ayrı bir
`.h` yazılmaz; paylaşılan bildirimler `nax.h` içinde, alan bazlı olanlar alan
başlıklarında toplanır.
