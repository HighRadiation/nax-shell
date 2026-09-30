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

### Test durumu

| Grup | Vaka |
|---|---|
| sınıflandırıcı korpusu | 62 |
| genişletme korpusu | 52 |
| sözcük ayırıcı korpusu | 44 |
| ayrıştırıcı korpusu | 40 |
| boru ile bütünleşik | 98 |
| sahte terminal (pty) | 26 |

Hepsi normal ve denetleyicili derlemede ayrı ayrı koşuyor. Takım kendi kendini
de denetliyor: sistem makroları ile sabit çakışması taraması ve kod normu
denetimi `make check` içinde.

## Sırada

### M2.3 — protokol ve yardımcı süreç iskeleti

Tel biçimi [PROTOCOL.md](PROTOCOL.md)'de yazılı; bu taş onu hayata geçiriyor.
Henüz gerçek bir model yok — karşı tarafta sahte bir süreç var, çünkü önce
**dayanıklılık** ölçülmeli.

- `src/ai/proto.c` — çerçeve kodlama ve çözme
- satır tabanlı okuma/yazma; `poll` üzerinde EINTR döngüsü
  ([FINDINGS.md](FINDINGS.md)'de açık bir madde)
- yük için base64
- yardımcı sürecin başlatılması ve gözetimi: yeniden doğma, zaman aşımı,
  çökme, hiç cevap vermeme
- `test/fake_naxd.py` — **kötü davranış kipleriyle**: yavaş cevap, bozuk
  çerçeve, cevabın ortasında ölme, hiç cevap vermeme, aşırı büyük yük

Bu taşın asıl sorusu şu: yardımcı süreç ölürse, donarsa ya da saçmalarsa
**kabuk sağlam kalıyor mu.** Kabuk hiçbir koşulda yardımcı süreç yüzünden
kilitlenmemeli.

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
