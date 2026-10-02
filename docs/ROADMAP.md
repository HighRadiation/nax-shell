# Yol haritası

Bu dosya **nerede kaldığımızı** ve **sırada ne olduğunu** tutar. Tasarım
gerekçeleri burada değil, ilgili dokümanlarda: mimari
[ARCHITECTURE.md](ARCHITECTURE.md), karar mantığı
[CLASSIFIER.md](CLASSIFIER.md), tel biçimi [PROTOCOL.md](PROTOCOL.md),
ayarlar [CONFIG.md](CONFIG.md), gizlilik [PRIVACY.md](PRIVACY.md), kod
kuralları [NORMS.md](NORMS.md), ertelenen işler
[FINDINGS.md](FINDINGS.md).

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

Tel biçimi ([PROTOCOL.md](PROTOCOL.md)) hayata geçti. Henüz gerçek model yok;
karşı tarafta **kasten kötü davranan** bir taklit var, çünkü bu taşın sorusu
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

### Test durumu

| Grup | Vaka | Mutasyon |
|---|---|---|
| sınıflandırıcı korpusu | 62 | 15/15 |
| genişletme korpusu | 52 | — |
| sözcük ayırıcı korpusu | 44 | — |
| ayrıştırıcı korpusu | 40 | — |
| **protokol korpusu** | **40** | **19/20** |
| **satır okuma senaryoları** | **14** | **14/17** |
| **yardımcı süreç senaryoları** | **23** | **23/23** |
| **gerçek daemon (Python)** | **20** | — |
| **tel biçimi çaprazlama** | **36** | — |
| **maskeleyici** | **47** | **17/17** |
| **bağlam halkası** | **26** | — |
| boru ile bütünleşik | 124 | — |
| sahte terminal (pty) | 35 | — |

Yakalanmayan dört mutasyon davranışı hiç değiştirmiyor; gerekçeleri kodda ve
[FINDINGS.md](FINDINGS.md)'de yazılı.

## Sırada

### M4 — tamamlama

- `<<` yönlendirmesi (girdi yolunu değiştirdiği için ayrı iş)
- `&& || ;` işleçleri
- dosya adı genişletmesi (globbing)
- `anthropic` bağdaştırıcısı
- **çevrimdışı için belirlenimci niyet→komut tablosu**: internet yokken
  yavaş bir yerel model beklemek yerine, sık isteklerin doğrudan karşılığı
- yerel model erişimi

## Temizlik borcu

Depoda iki eski yedek dal duruyor; işi bitti, silinebilir:

```
yedek/rebase-oncesi
yedek/yazar-degisiminden-once
```
