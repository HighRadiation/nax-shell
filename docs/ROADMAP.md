# Yol haritası

Bu dosya **nerede kaldığımızı** ve **sırada ne olduğunu** tutar. Tasarım
gerekçeleri burada değil, ilgili dokümanlarda: mimari
[ARCHITECTURE.md](ARCHITECTURE.md), karar mantığı
[CLASSIFIER.md](CLASSIFIER.md), tel biçimi [PROTOCOL.md](PROTOCOL.md),
ayarlar [CONFIG.md](CONFIG.md), gizlilik [PRIVACY.md](PRIVACY.md), kod
kuralları [NORMS.md](NORMS.md), ertelenen işler
[FINDINGS.md](FINDINGS.md), nereye gidildiği [VISION.md](VISION.md).

## Nasıl çalışıyoruz

Kilometre taşları **sırayla ve kapanarak** ilerliyor: bir taş bitince durulur,
kontrol edilir, sonra bir sonrakine geçilir. Yarısı bitmiş iki taş yerine
kapanmış bir taş tercih edilir.

Her taşın bitmiş sayılması için üç şart:

1. `make check` sıfır hata — hem normal hem denetleyicili derlemede.
2. **Yeşil test kanıt değildir.** Her test grubu, koda kasten hata sokularak
   doğrulanır (mutasyon testi). Kaçan mutasyon ya bir test boşluğu ya bir kod
   bulgusudur; ikisi de kapatılır, kapatılamıyorsa
   [FINDINGS.md](FINDINGS.md)'ye yazılır.
3. Dokümanlar koda göre **doğru** — plandan sapma olduysa sapma yazılır.

## Bitenler

### M0 — iskelet

Okuma döngüsü, renkli prompt, kalıcı geçmiş, sinyaller. Ctrl-C yarım satırı
atıp temiz prompt verir, Ctrl-\ yok sayılır, Ctrl-D çıkar.

### M1 — çekirdek kabuk

Sözcük ayırıcı, ayrıştırıcı, genişletme ve çalıştırıcı. n aşamalı borular, üç
yönlendirme (`<` `>` `>>`), altı yerleşik (`cd`, `echo`, `pwd`, `export`,
`unset`, `exit`). Davranışın tamamı bash ile karşılaştırılarak ölçüldü
(yaklaşık 70 karşılaştırma). 400 boru hattı sonrası açık fd sayısı 3, zombi
yok.

### M2.1 — sınıflandırıcı

Kabuk her satırı kendisi sınıflandırıyor: hiçbir işaret, hiçbir kip yok.
Normal komutlar AI'a hiç uğramıyor — `ls -la` yazıldığında ne gecikme ne ücret
oluşuyor.

Şekil vetoları, PATH'te gerçekten var olan İngilizce emir kiplerini
(`find`, `make`, `who`, `free`, `open`, `sort`…) doğal dil cümlelerinden
ayırıyor. Karar sırası ve her vetonun gerekçesi
[CLASSIFIER.md](CLASSIFIER.md)'de.

### M2.2 — yerel yazım düzeltmesi

`celar` → `clear`, AI'a hiç gitmeden. Aday kümesi yerleşikler + PATH,
sıralama önce uzaklık sonra geçmiş sıklığı; uzaklık ölçüsü yer değiştirmeyi
tek adım sayıyor.

**Geri dönüşü olmayan komutlar önerilir ama düzenleme tamponuna
konulmaz.** Tampona konan öneri tek Enter'la koşar ve `rm -rf` için o fazla
yakın.

### M2.3 — protokol ve yardımcı süreç iskeleti

Tel biçimi ([PROTOCOL.md](PROTOCOL.md)) hayata geçti. Bu taşta gerçek model
henüz yoktu; karşı tarafta **kasten kötü davranan** bir taklit vardı (gerçek
model ölmeyi sipariş üzerine yapmaz), çünkü bu taşın sorusu
"cevap doğru mu" değil: **yardımcı süreç ölürse, donarsa ya da saçmalarsa
kabuk sağlam kalıyor mu.**

- `proto.h` + `b64.c` + `proto.c` — kayıt kurma ve çözme. Serbest metin
  `b64:` önekiyle; çözmede tek tampon, base64 yerinde çözülüyor
- `lineio.c` — akıştan tam satır toplama, dört durum. Aşırı uzun satırda
  bir sonraki satıra kadar atlanıyor, yani bir bozuk kayıt akışı
  zehirlemiyor
- `naxd.h` + `naxd.c` + `naxd_wait.c` — süreç yaşam döngüsü, `poll` + kendine
  boru, zaman aşımı, yeniden doğma, hata çıkışının günlüğe gitmesi
- `test/fake_naxd.py` — **17 kötü davranış kipi**

Ölçülen üç dayanıklılık kuralı:

| Durum | Kabuğun cevabı |
|---|---|
| Süreç ölür (yazarken ya da okurken) | Bağlantı kapatılır, kabuk yaşar; SIGPIPE yok sayılı olduğu için yazma hata döndürür |
| Süreç hiç cevap vermez | Zaman aşımı; `CANCEL` gönderilir, satır kullanıcıya geri verilir |
| Süreç **okumayı bırakır** | Yazma da sınırlı beklenir. Bu olmadan kabuk `write` içinde sonsuza kadar asılı kalıyordu — okuma tarafındaki zaman aşımı devreye girmiyor |

Kullanıcının `Ctrl-C` ile beklemeyi kesmesi **kendine boru** ile gözleniyor:
kesme işleyicisi o boruya bir bayt yazıyor, çünkü sinyal bağlamında güvenle
yapılabilecek tek iş bu. "Bayrak set et" tek başına `poll`'u uyandırmaz.

### M2.4 — gerçek yardımcı süreç ve ilk sağlayıcı

Kabuk artık doğal dili gerçekten çalıştırıyor. Yardımcı süreç yalnızca Python
standart kütüphanesiyle yazıldı: kullanıcının paket kurması gerekmiyor.

**Soru ile istek ayrımı** bu taşın asıl işi. Sınıflandırıcı doğal dil dediği
her satırı niyet olarak gönderiyor, ama doğal dilin iki türü var:

| Satır | Ne istiyor | Kabuk ne yapıyor |
|---|---|---|
| `eski logları sil` | komut | Öneriyi yazar, düzenleme satırına hazırlar |
| `bu hata ne demek` | cevap | Metni ekrana basar |

Ayrımı model yapıyor ve cevabı **iki başlıktan** biriyle veriyor: `CMD <komut>`
ya da `ANS <cevap>`. Serbest metinden komut çıkarmaya çalışmak tahmin işi
olurdu ve tahmin eden bir kabuk güvenilmez.

**Başlık yoksa cevap sayılır.** Sıra bilinçli: yanlış okunan bir metni ekrana
basmak zararsız, yanlış okunan bir komutu düzenleme satırına yazmak değil.

### Üç güven kuralı

**Risk kararını kabuk kendi verir.** Yardımcı süreç bir `danger` ipucu
gönderiyor ama tek başına ona güvenilmiyor; kabuk komutun başını kendi
listesiyle de denetliyor. Testi karşı tarafın *yalan söylediği* durumu
ölçüyor: `danger=0` dense bile `rm -rf` tampona konmuyor.

**Anahtar yoksa kabuk düz kabuk olur.** Sebep oturumda bir kez yazılır, sonra
satır bash'in yaptığı şeye düşer: ilk sözcük için `command not found` ve 127.
Anahtarsız durum bir hata hâli değil, desteklenen bir çalışma biçimi.

**Düşme tetikleyicisi "internet var mı" değil "çağrı patladı mı".** Üst üste
iki bulut hatası yerel modele geçirir; başarılı çağrı sayacı sıfırlar.

### Testler ağa çıkmıyor

Karşı tarafta sahte bir model servisi var ve kipini istenen **model adından**
okuyor, yani yeni bir yapılandırma alanı gerekmiyor. İki çaprazlama testi
eklendi:

- Python tarafı C'nin vaka dosyasını okuyor, yani korpus artık tek bir
  uygulamanın değil **biçimin** testi. İlk koşumda üç ayrışma yakalandı.
- C istemcisi **gerçek** Python daemon'ıyla el sıkışıyor. Taklitle konuşmak
  "kendi yazdığımla anlaşıyorum" demekti.

### M3 — paylaşılan durum

Projeyi ayırt eden ikinci şey hayata geçti: içerideki AI çalışma dizinini, son
komutları, çıkış kodlarını ve git dalını **sormadan** biliyor. Terminalde ayrı
bir program olarak koşan bir yardımcı bunları bilemez.

```
nax ~/nax-shell $ ctx
--- her istekte gidiyor ---
dizin: ~/nax-shell
git dali: main
isletim sistemi: Linux
en sik kullanilan: git 412, make 180, ls 97
son komutlar:
  [0] echo bir
  [127] boylebirkomutyok
--- yalniz oturum acilisinda gitti ---
ortam degiskeni adlari: PATH, HOME, TERM, ...
--- bunun disinda hicbir sey ---
```

### Gizlilik sözleşmesi uygulandı

`docs/PRIVACY.md` bir belge değil, uygulanan bir sözleşme. Dört kuralı:

| Kural | Nasıl uygulandı |
|---|---|
| Temizleme **kabuk tarafında** | Sekiz kalıp, veri yardımcı sürece verilmeden önce taranıyor. Hatalı ya da ele geçirilmiş bir süreç, hiç almadığı veriyi sızdıramaz |
| Boşlukla başlayan satır bağlama girmez | Kullanıcının zaten bildiği bir hareket, bağlamdan kaçmanın yolu oluyor |
| Ortam değişkenlerinin **yalnızca adları** | Değerler asla; ad listesi oturum açılışında bir kez |
| Belirli dizinlerde AI **tamamen kapalı** | Karar gönderen tarafta, yani veri karşı tarafa hiç ulaşmıyor |

**Maskeleme girişte yapılıyor, çıkışta değil.** İki sebebi var: `nax ctx`
gidecek baytları aynen basmak zorunda ve maskelemeyi gönderme anına bırakmak
iki ayrı yol üretirdi; ikincisi daha önemli — temizlenmemiş bir sır bellekte
hiç durmuyor.

**`nax ctx` gizlilik iddiasını denetlenebilir kılan tek özellik.** Belgeye
güvenmek zorunda değilsin, bakabilirsin. Testi de bunu kullanıyor: sahte bir
sır ekip çıktıda görünmediğini doğruluyor.

### M4 — tamamlama

Kabuk dili tamamlandı ve AI tarafına iki katman eklendi.

| Kalem | Durum |
|---|---|
| `<<` yönlendirmesi | bitti |
| `&& \|\| ;` işleçleri | bitti |
| dosya adı genişletmesi | bitti |
| `anthropic` bağdaştırıcısı | bitti |
| çevrimdışı niyet tablosu | bitti |
| yerel model erişimi | altyapı bitti, **gerçek modele karşı denenmedi** |

Son satır dürüstlük gereği böyle: yapılandırma, düşme kuralı ve istek yolu
sahte bir sunucuya karşı ölçüldü, ama bir `ollama` sunucusuyla gerçekten
konuşulmadı. Aynı şey bulut tarafı için de geçerli — ayrıntısı aşağıda.

### Dilin tamamlanması

`;`, `&&` ve `||` için boru hattının **üstüne** bir katman eklendi: liste.
Tek hatlık listede sarmalayıcı yazılmadığı için eski dökümlerin hiçbiri
değişmedi. Anlamların tamamı bash ile karşılaştırıldı — atlanan hattın durum
kodunu değiştirmemesi dahil.

`<<` gövdesi **ayrıştırmadan sonra** toplanıyor, çünkü kaç satır okunacağı
ancak sınırlayıcı bilindikten sonra belli oluyor. Bu, kabuğun girdi yolunu
değiştiren tek özellik.

Dosya adı genişletmesinde hangi yıldızın desen olduğu **karakter bazında**
biliniyor: genişletici her yazdığı karakterin yanına bir maske biti koyuyor.
Ölçüldü, başka yolu yok:

```
echo "*.c"           -> harf        X="*.c"; echo $X    -> DESEN
echo "a"*.c          -> "a" harf, yıldız desen (aynı sözcükte)
```

Son satır sözcük ya da parça bazında bir bayrağı diskalifiye ediyor.

### Çevrimdışı katman

`docs/CONFIG.md`'nin tezi uygulandı: çevrimdışı değerin büyük kısmı dil
modelinden değil, sıfır gecikmeli belirlenimci katmandan geliyor. O katman
artık dört parça — yazım düzeltmesi, PATH çözümlemesi, sınıflandırıcı ve
**elle yazılmış niyet tablosu.**

Tablo modelden *ve yazım düzeltmesinden* önce bakılıyor. İkinci sıra ölçülerek
bulundu: ters olduğunda "kac dosya var" satırı tabloya hiç ulaşmıyor, `kac`
sözcüğü için `tac` önerisi alıyordu. Tablonun tanıdığı satır tanım gereği
doğal dildir, yazım hatası değil.

### Test durumu

| Grup | Vaka | Mutasyon |
|---|---|---|
| sözcük ayırıcı | 44 | — |
| ayrıştırıcı | 56 | 13/13 |
| genişletme | 52 | — |
| **dosya adı genişletmesi** | **37** | **16/16** |
| sınıflandırıcı | 64 | 15/15 |
| maskeleyici | 47 | 17/17 |
| bağlam halkası | 26 | — |
| **çevrimdışı tablo** | **33** | **14/14** |
| protokol | 40 | 19/20 |
| satır okuma | 14 | 14/17 |
| yardımcı süreç | 23 | 23/23 |
| gerçek daemon (Python) | 25 | — |
| tel biçimi çaprazlama | 36 | — |
| boru ile bütünleşik | 168 | — |
| sahte terminal (pty) | 39 | — |

Toplam 30 grup, hepsi normal **ve** denetleyicili derlemede. Yakalanmayan dört
mutasyon davranışı hiç değiştirmiyor; gerekçeleri kodda ve
[FINDINGS.md](FINDINGS.md)'de yazılı.

### Ölçülmeyen tek şey: cevap kalitesi

Protokol, dayanıklılık, gizlilik ve kabuk dili ölçüldü. **Modelin ürettiği
komutların işe yarayıp yaramadığı ölçülmedi** — bütün AI testleri sahte bir
sağlayıcıya karşı koşuyor.

Bu bilinçli: ağ, anahtar, ücret ve rastgelelik testlere girmemeli. Ama açık bir
boşluk ve kapatmanın tek yolu gerçek bir anahtarla oturup kullanmak. Plan
"bitti" dediğinde ürün bitmiş olmayacak; o adım plan dışıdır.

### İlk gerçek koşu: üç hata, üçü de planda yoktu

2026-10-02'de kabuk ilk kez gerçek bir anahtarla çalıştırıldı. Cevap kalitesi
hâlâ ölçülmedi, çünkü **ilk üç deneme modele hiç ulaşmadı.** Çıkan üç hata
yukarıdaki "ölçülmeyen tek şey" bölümünün neden yazıldığını gösteriyor:

| Ne oldu | Gerçek sebep | Nerede kapandı |
|---|---|---|
| `cd /tmp` yazıldıktan sonra AI tamamen durdu | Yardımcı sürecin betiği **göreli** yolla çağrılıyordu. `cd` bir kabukta en sık kullanılan komut, yani bu her oturumda yaşanacak bir hataydı | `src/ai/bridge.c` — yol `/proc/self/exe` ile ikilinin yanından bulunuyor |
| Her istek `servis 403 dondurdu` | Cloudflare'in `1010` kodu: urllib'in varsayılan imzası bot listesinde, istek API'ye hiç ulaşmıyor. Ekranda yalnızca durum kodu yazdığı için sebep görünmüyordu | `naxd/provider.py` — `User-Agent` başlığı, ve hata gövdesinden tek satırlık gerekçe |
| `dun degisen dosyalari zip'le` → `find ...` önerildi, zip yok | Çevrimdışı tablo satırın yarısını açıklayan kayıtla cevap verdi; "zip'le" sözcüğü yok sayıldı | `src/ai/offline.c` — kazanan kayıt satırın **tamamını** karşılamak zorunda |

Üçünün ortak yanı şu: hiçbiri bir test tarafından yakalanamazdı, çünkü üçü de
testlerin bilinçli olarak dışında bıraktığı yerde duruyordu — gerçek çalışma
dizini, gerçek ağ, gerçek cümle. Her üçü için şimdi vaka var.

## Sırada

**Plandaki bütün kilometre taşları bitti.** Bu, projenin bittiği anlamına
gelmiyor — plan bir iskeleydi ve iskele tamamlandı.

Geriye üç tür iş kalıyor:

**1. Gerçek kullanım.** Yukarıdaki "ölçülmeyen tek şey" bölümü. Gerçek bir
anahtarla oturup kullanmak, ve çıkan şeyi ölçmek. Bu adım plan dışıdır çünkü
planın içinden yapılamaz.

**2. [FINDINGS.md](FINDINGS.md)'deki açık maddeler.** Hiçbiri engelleyici
değil ama "bitti" dedikten sonra da iş olduğunu gösteriyorlar: `~user`,
`IFS`, atama öneki olmadan değişken (`X=1`), `$$`, `${VAR:-x}`, `echo -e`,
yardımcı sürecin kurulum yolu.

**3. Kullanımın göstereceği şeyler.** Bugüne kadarki tecrübe şunu söylüyor:
gerçek hatalar planda yazmıyordu. `R_OK` makro çakışması, `t_field` ad
çakışması, yazma tıkanmasında kilitlenme, maskeleyicide yanlış sıra, çevrimdışı
tablonun yazım düzeltmesiyle çakışması, ve ilk gerçek koşunun çıkardığı üç hata
(yukarıda) — hiçbiri öngörülmüştü, hepsi kod yazılırken ya da **kullanılırken**
çıktı. Dördüncü tür iş budur ve listelenemez.

## Temizlik borcu

Depoda iki eski yedek dal duruyor; işi bitti, silinebilir:

```
yedek/rebase-oncesi
yedek/yazar-degisiminden-once
```

**Depo public'e açılmadan önce bilinmesi gereken:** ikisi de yalnızca yerelde
duruyor, `origin`'e hiç gitmedi. Ama `yedek/yazar-degisiminden-once` dalı
**başka bir kimlikle** atılmış commit'ler içeriyor (adından da belli: yazar
değişiminden önce). `git push --all` ya da `git push --mirror` o dalı da
gönderir ve o kimlik public olur. Depoyu açarken `git push origin main`
biçiminde dalı adıyla göndermek, ya da bu dalları önce silmek gerekiyor.
