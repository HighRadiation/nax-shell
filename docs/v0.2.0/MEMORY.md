# Kalıcı bellek

**Hedef:** nax kullanıcının kim olduğunu bilsin. Oturum içi bağlam değil,
oturumlar arası kalan bilgi.

Durum: planlandı, ölçülmedi. Kod yok.

## Yeni bir sistem değil, var olanın diske yazanı

Bugün zaten bir bağlam katmanı var ve çalışıyor. `src/ai/ctx.c` son komutları
ve çıkış kodlarını bir halka tamponda tutuyor; `src/ai/ctx_facts.c` çalışma
dizinini, git dalını ve kirliliğini, işletim sistemi adını, terminal
genişliğini ve ortam değişkeni adlarını topluyor. `ctx_block()` bunu modele
giden bloğa çeviriyor, `ctx` yerleşiği kullanıcıya gösteriyor.

Kalıcı bellek **bu yapının ikinci bir kaynağı** olmalı: aynı blok, biri
oturumdan biri diskten. Paralel bir "bellek sistemi" kurmak aynı işi iki yerde
yapmak demek.

Ayrım şu: oturum bağlamı **gözlemlenir** (komutlar, çıkış kodları, dizin),
kalıcı bellek **söylenir ya da çıkarılır** (kim olduğu, ne kullandığı, neyi
tercih ettiği). İkincisinin ömrü uzun olduğu için yanlışı da uzun yaşar.

## Nerede durur

**Kabukta, `naxd`'de değil.**

`naxd` yeniden başlatılabilir bir süreç: saçmalarsa ihlal sayılıp öldürülüyor
ve yenisi açılıyor ([../ARCHITECTURE.md](../ARCHITECTURE.md)). Kalıcı belleği
oraya koymak, dayanıklılık mekanizmasının belleği silmesi demek. Bağlam bugün
de kabukta toplanıp `naxd`'ye gönderiliyor; bellek aynı yönü izler.

## En büyük risk: gizlilik

`src/ai/redact.c` bugün **giden isteği** temizliyor — bulut anahtarları,
imzalı token'lar, özel anahtar blokları, URL içindeki kimlik bilgileri, uzun
rastgele diziler, bayrak değerleri. Bu kod isteğin yolunda duruyor.

Kalıcı bellek yeni bir yol açıyor: **diske yazan bir yol.** Temizlenmemiş bir
sır bellekte kalırsa bir kez değil, her oturumda sızar. Bu gözlem
[../FINDINGS.md](../FINDINGS.md)'de zaten kayıtlı ve bu özellik onu tam
ortasından ilgilendiriyor.

Üç kural bundan çıkıyor ve kod yazılmadan kabul edilmesi gerekiyor:

- **Yazma yolu da `redact.c`'den geçer.** İstek yolundan geçmesi yetmez.
- **Dosya insan okuyabilir olur.** Kullanıcı ne hatırlandığını görebilsin,
  elle düzeltebilsin, silebilsin. İkili bir biçim bu üçünü de engeller.
- **Dosya izni `0600`.** `nax.conf` için verilen karar burada da geçerli.

[../PRIVACY.md](../PRIVACY.md)'nin kapsamı bu özellikle genişliyor; o dosya
bugün yalnız "ne gönderiliyor" sorusunu cevaplıyor, "ne saklanıyor" sorusunu
da cevaplamak zorunda kalacak.

## Çözülmemiş sorular

**Ne hatırlanır.** Sınırsız büyüyen bellek istem bütçesini yer ve modeli
zayıflatır — ilgisiz yüz satır, ilgili beş satırı bastırır. Halka tampon
oturum için doğru cevaptı; kalıcı bellek için karşılığı ne, ölçülmeden
bilinmiyor.

**Kim yazar.** Kullanıcı mı açıkça söyler ("beni şöyle bil"), model mi
çıkarır. Model çıkarırsa yanlış çıkarım kalıcı olur ve kullanıcı onu
görmediği sürece düzeltemez. Bu, "insan okuyabilir dosya" kuralının asıl
gerekçesi.

**Çakışma.** İki nax oturumu aynı anda açıkken aynı dosyaya yazarsa ne olur.
Kabuk bugün tek oturumluk davranıyor; bu varsayım ilk kez burada kırılıyor.

**Unutma.** Yanlış bir bellek satırını silmenin yolu ne. `ctx` yerleşiğinin
kalıcı bellek için karşılığı gerekiyor — göstermek, silmek, kapatmak.
