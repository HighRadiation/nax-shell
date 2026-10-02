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

### Test durumu

| Grup | Vaka | Mutasyon |
|---|---|---|
| sınıflandırıcı korpusu | 62 | 15/15 |
| genişletme korpusu | 52 | — |
| sözcük ayırıcı korpusu | 44 | — |
| ayrıştırıcı korpusu | 40 | — |
| **protokol korpusu** | **40** | **19/20** |
| **satır okuma senaryoları** | **14** | **14/17** |
| **yardımcı süreç senaryoları** | **22** | **23/23** |
| boru ile bütünleşik | 101 | — |
| sahte terminal (pty) | 29 | — |

Yakalanmayan dört mutasyon davranışı hiç değiştirmiyor; gerekçeleri kodda ve
[FINDINGS.md](FINDINGS.md)'de yazılı.

## Sırada

### M2.4 — gerçek yardımcı süreç ve ilk sağlayıcı

- `naxd` — yalnızca Python standart kütüphanesi (bağımlılık kurmak
  gerekmemeli)
- `compat` sağlayıcı bağdaştırıcısı (Groq ile)
- `nax.conf` okuma; biçim [CONFIG.md](CONFIG.md)'de
- **soru ile istek ayrımı**: "bu ne demek" cevap ister, "eski logları sil"
  komut ister; ikisi aynı yoldan geçmemeli

### M3 — paylaşılan durum

Projeyi ayırt eden ikinci şey: içerideki AI cwd'yi, son komutları, `$?`'yi ve
git dalını **sormadan** bilir.

- oturum bağlamı için halka tampon
- `HELLO` anlık görüntüsü: cwd, git dalı, son durum, son komutlar
- maskeleme; kurallar [PRIVACY.md](PRIVACY.md)'de
- `nax ctx` — bağlamda ne olduğunu kullanıcıya gösteren komut
- `EXPLAIN` yolu: tek başına `?` son hatayı açıklatır (sınıflandırıcıda hazır,
  bağlı değil)

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
