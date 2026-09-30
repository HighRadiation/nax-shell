# Kod normları

Bu dosya bu depodaki kod stili sözleşmesidir. Tartışmaya açık değil; yeni
kod bunlara uyar.

## İsimlendirme

**Dosya, fonksiyon ve değişken adları İngilizce. Yorumlar Türkçe.**

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
| `exp_` | genişletme |
| `ex_` | çalıştırıcı |
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
