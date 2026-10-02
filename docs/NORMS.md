# Kod normları

Bu dosya bu depodaki kod stili sözleşmesidir. Tartışmaya açık değil; yeni
kod bunlara uyar.

## İsimlendirme

**Dosya, fonksiyon ve değişken adları İngilizce. Yorumlar Türkçe.**

Bu kural **`Makefile` ve betikler için de geçerli**: hedef adları, değişken
adları ve ekrana basılan etiketler İngilizce (`all`, `check`, `clean`, `cc`,
`link`). Kuralın sebebi okuyanı tahmin etmeye bırakmamak: `make` ile gelen
sözleşme İngilizce, araya Türkçe bir hedef adı koymak o sözleşmeyi bozar.
Açıklamalar burada da Türkçe, kullanıcıya yazılan hata mesajları da
(`HATA: readline basliklari bulunamadi`) — programın her yerinde olduğu gibi.
Ayrım şu: **terim İngilizce, cümle Türkçe.**

**Kullanıcıya gösterilen her hata satırı, bakılacak yeri söylemek
zorunda.** Durum kodu tek başına yeterli değil; gerekçe de gerekiyor.
Bu kural bir desenden doğdu: bir gün içinde üç hata satırı aynı şekilde
eksik çıktı — `AI baslatilamadi` (neden olduğunu söylemiyordu),
`servis 403 dondurdu` (sebep gövdede duruyordu, çöpe atılıyordu),
`yanit bos` (hiçbir şey loglanmıyordu). Üçü de kullanıcıyı yanlış yere
baktırdı; 403 görünce insan ilk iş anahtarını suçluyor. Bu kod tabanının
iç doğruluğu güçlü, insana durumu anlatması tarihsel olarak zayıf — kural
bu yüzden yazılı. Tam yanıt günlüğe, tek satırlık gerekçe ekrana.

**Kod dosyaları tamamen ASCII.** Türkçe karakterler ASCII karşılıklarıyla
yazılır, uzun tire yerine `-` kullanılır. Dokümanlar (`docs/*.md`) bu kuralın
dışında — orada tam Türkçe yazım geçerli.

Kural "Türkçe karakter yok" değil "ASCII" olarak yazılı, çünkü ilki eksikti:
kodda hiç Türkçe harf yoktu ama her dosya başlığında bir uzun tire vardı ve
yalnızca Türkçe harf arayan denetim onu kaçırıyordu.

Fonksiyon adları modül önekiyle yazılır, böylece bir adı gördüğünde hangi
dosyada yaşadığı belli olur:

| Önek | Modül |
|---|---|
| `buf_` | büyüyebilen metin tamponu |
| `ln_` | satır okuma, prompt, geçmiş |
| `cls_` | sınıflandırıcı |
| `fix_` | yazım düzeltme |
| `path_` | PATH çözümleme |
| `lex_` | sözcüklere ayırma |
| `ast_` | ayrıştırıcı |
| `field_` | genişletme sonucu alanlar |
| `xcmd_` | genişletilmiş komut |
| `ex_` | çalıştırıcı |
| `redir_` | yönlendirme uygulama |
| `sig_` | sinyaller |
| `exp_` | genişletme |
| `ex_` | çalıştırıcı |
| `redir_` | yönlendirme uygulama |
| `bi_` | yerleşik komutlar |
| `ses_` | oturum bağlamı |
| `red_` | sır temizleme |
| `nxd_` | yardımcı süreç istemcisi |
| `pr_` | protokol |
| `sig_` | sinyaller |

Bir dosyanın içinde kalan yardımcılar `static` olur ve önek almaz.

## Yorumlar

**Her fonksiyonun başlık satırının bir üstünde**, ne yaptığını söyleyen bir
yorum bulunur.

**Fonksiyon gövdesinde yorum bulunmaz.** Bu kuralın doğal sonucu şudur:
fonksiyon, kendini anlatacak kadar küçük olmak zorundadır. Gövde içinde
açıklama ihtiyacı duyuyorsan orada ayrı bir fonksiyon vardır.

Dosyanın başında "neden var" bloğu bulunur: modülün görevi, varsa kritik
tuzaklar ve tasarım gerekçesi.

```c
/* Bastaki bosluklari atlar; ilk bosluk olmayan karakteri gosterir. */
static const char	*skip_blank(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}
```

## Biçim

- Dönüş tipi ve fonksiyon adı aynı satırda, arada sekme.
- Açılış süslü parantezi **kendi satırında**.
- `return` değeri parantez içinde: `return (0);`
- Girinti sekme ile.
- Değişken bildirimleri fonksiyonun başında, gövde ondan sonra.

## Yapı tanımları başlıklarda durur

`struct`, `enum` ve `union` tanımları `.c` dosyalarına yazılmaz; başlık
dosyalarında durur. Bu, yalnızca tek bir modülün kullandığı iç yapılar için de
geçerlidir — örneğin sözcük ayırıcının geçici durumu yalnızca `lexer.c`
tarafından kullanılır ama tanımı `parse.h` içindedir.

Gerekçe okunabilirlik: bir tipi aradığında nereye bakacağını bilirsin, ve bir
modülün veri şekli uygulamasından ayrı okunabilir.

| Başlık | İçerdiği yapılar |
|---|---|
| `src/nax.h` | oturum durumu, metin tamponu |
| `src/parse.h` | tırnak kipi, sözcük parçası, token, sözcük ayırıcı durumu, hata bildirimi |
| `test/test.h` | test sayacı |

## Sabit adları sistem makrolarıyla çakışmamalı

Enum sabitleri ve makrolar **modül önekli** olur. Kısa ve genel adlar POSIX
makrolarıyla çakışır.

Bu kural ölçülmüş bir hatadan geliyor: `R_OK` adlı bir enum sabiti
`<unistd.h>` içindeki `access()` makrosuyla çakıştı. Makro olduğu için ön
işlemci koddaki her `R_OK` metnini `4` ile değiştirdi — yani başka bir sabitin
değeriyle. Ortaya geçerli kod çıktığı için derleyici **tek bir uyarı vermedi**;
hata yalnızca davranışta göründü ve bash ile karşılaştırma testi yakaladı.

`make check` bu sınıfı otomatik denetler: başlıklardaki her büyük harfli sabit
için, sistem başlıklarından *sonra* makro olarak tanımlı mı diye bakar. Denetim
kasten çakışma sokularak doğrulandı.

## Modülerlik

Bir fonksiyonun içinde başka bir fonksiyona ait iş bulunmaz. Örnek: "satır
çıkış isteği mi" sorusunu yanıtlayan fonksiyon, boşluk atlama işini kendi
gövdesinde yapmaz; o işi yapan fonksiyonu çağırır.

Dosya sayısında denge aranır: ne her şeyi tek dosyaya yığmak, ne her fonksiyon
için ayrı dosya açmak. Aynı gerekçeyle her `.c` için ayrı `.h` yazılmaz.

## Bellek

**Sıfır sızıntı.** Bu bir hedef değil, kabul kriteri: hiçbir kilometre taşı,
denetleyici temiz rapor vermeden bitmiş sayılmaz.

Sahiplik kuralı: her modül kendi ürettiği tipi kendisi serbest bırakır ve
serbest bırakma fonksiyonu tipin yaşadığı dosyada durur. Güncel sahiplik
tablosu [ARCHITECTURE.md](ARCHITECTURE.md) içinde.

Bu ortamda `valgrind` yok. Doğrulama derleyicinin adres ve tanımsız davranış
denetleyicileriyle yapılır:

```
make asan     # denetleyicili ikiliyi uretir
make check    # testleri hem normal hem denetleyicili ikilide kosar
```

## Normlar suite içinde denetlenir

Bu dosyadaki kuralların denetimi `make check` içinde koşar; elle koşulan
denetim, koşulmayan denetime dönüşür.

Bu bir kez yaşandı: kaynaklar alt klasörlere taşınırken makro çakışma
denetiminin `src/*.h` deseni yalnızca tek başlığı bulmaya başladı ve denetim
**hiçbir şey denetlemediği hâlde yeşil geçti**. Aynı taşımada ASCII denetimi de
kodda zaten var olan bir uzun tireyi ortaya çıkardı — eski desen yalnızca
Türkçe harf arıyordu, tipografik karakteri kaçırıyordu.

Altı alt denetimin her biri, kasten ihlal sokularak doğrulandı. Yeni bir
denetim eklendiğinde aynı doğrulama yapılır.

`make check` üç test grubunu da koşar: birim testleri modülleri doğrudan
çağırır, boru ile beslenenler etkileşimsiz yolu, sahte terminal testleri
etkileşimli yolu kapsar. Yalnızca birini koşmak kodun bir kısmını ölçmeden
bırakır.

## Yeşil test kanıt değildir

Bir test grubu ilk koşuda yeşil geldiğinde soru şu olur: gerçekten bir şey
ölçüyor mu? Cevabı **mutasyon denemesiyle** alınır — koda kasıtlı bir hata
sokulur ve testin onu yakaladığı görülür:

```
# ornek: >> operatorunu tanimayan bir lexer testleri patlatmali
```

Sözcük ayırıcıda altı mutasyon denendi (operatör tanımama, tırnağı yanlış
kiple işaretleme, kapanmamış tırnağı sessizce geçme, kaçış karakterinin kip
düşürmemesi, boş tırnağın parça üretmemesi, operatörün sözcüğü bitirmemesi) ve
her biri 1-7 vaka patlattı. Yeni bir test grubu yazıldığında aynı deneme
yapılır; yakalamayan bir test grubu yoktur sayılır.

## Uyarılar

`make` uyarıları hata sayar. Taban `-Wall -Wextra -Werror`, üstüne gölgeleme,
değişken uzunlukta dizi, eksik prototip ve biçim dizesi denetimleri eklenir.

Hızlı bir deneme için `make W=0` gevşetir, ama depoya giren kod uyarısız
derlenir.

## Commit

Bol ve açıklayıcı commit. Her anlamlı alt adım bir commit olur.

Biçim `modül: ne yapıldı`:

```
lexer: tirnak icindeki kelimeleri tek sozcuk olarak topla
line: etkilesimsiz girdide readline yerine getline kullan
```

## Adlandırma yasağı

Bu depoda belirli iki kelime (bir okul adı ve bir ödev adı) hiçbir dosyada
geçmez — kaynak, doküman, commit mesajı ve README dahil.
