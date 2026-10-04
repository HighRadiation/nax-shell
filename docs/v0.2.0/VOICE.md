# Ses katmanı

**Hedef:** kabuk kullanıcıyı sesiyle dinlesin, cevabı metin olarak versin.

Girdi ses, çıktı metin. Bu bir karar, eksiklik değil: terminal okunan bir yer
ve metin cevap geri okunabilir, aranabilir, kopyalanabilir. Sesli cevap
sonraki bir iş, bu özelliğin parçası değil.

Durum: planlandı, ölçülmedi. Kod yok.

## Süreç modeli: kalıp zaten var

Tanıma motoru `naxd` ile aynı kalıba oturur — ayrı süreç, satır tabanlı
protokol, ölürse kabuk yaşar. [../PROTOCOL.md](../PROTOCOL.md) ve
[../ARCHITECTURE.md](../ARCHITECTURE.md)'deki süreç modeli ikinci bir tüketici
için yeniden yazılmak zorunda değil.

`whisper.cpp` bu kalıba uyuyor: C++, CPU'da çalışır, model dosyası dışında
kurulum istemez. Yerel çalıştığı için ses verisi makineden çıkmaz — bu
[../PRIVACY.md](../PRIVACY.md) açısından bulut tanımadan apayrı bir yerde
durur ve tercih sebebi.

Kabuk tarafında eklenen şey `src/ai/` içinde yeni bir köprü; `src/core/line.c`
satırın nereden geldiğini bilmek zorunda değil.

## Asıl problem: birinci yasanın ses karşılığı yok

Bu özelliğin zor kısmı tanıma değil.

"Hiçbir komut kullanıcı görmeden çalışmaz" bir **klavye ve ekran** yasası.
Komutu çalıştıran şey kullanıcının Enter'a basması; tampon (`rl_insert_text`)
tam bu yüzden var. Seste Enter yok.

Bu cevaplanmadan ses yolu yazılırsa yasa iptal olur — üstelik kodda hiçbir şey
kırılmadan, yani mevcut testlerin hiçbiri kızarmaz. Depodaki en güçlü söz o
yasa ve sessizce kaybolmaya en müsait yer burası.

Sorulacak soru: **Enter'ın yerine ne geliyor?**

### Üç aday, hiçbiri karar değil

**1. Ses yalnızca tamponu doldurur.** Tanınan cümle niyet hattından geçer,
çıkan komut tampona yazılır, çalıştıran yine Enter'dır. Yasa olduğu gibi
korunur, kabuk tarafında neredeyse hiç değişiklik gerekmez. Bedeli: "eller
serbest" vaadini vermez — konuşuyorsun ama onaylamak için klavyeye dönüyorsun.

**2. Sesli onay sözcüğü.** Komut okunur, kullanıcı "çalıştır" der. Eller
serbest kalır. Bedeli ağır: artık *tanıma hatası komut çalıştırabilir*. Yasa
"kullanıcı gördü" demekten "kullanıcının söylediğini sandık" demeye iner.
Geri dönüşsüz komutlarda bu kabul edilemez.

**3. Geri okuma + süre.** Komut okunur, n saniye içinde itiraz gelmezse
çalışır. Zaman aşımının onay sayılması yasanın tersi: sessizlik izin olur.

Birinci aday yasayı koruyor ama vaadi küçültüyor; ikincisi vaadi veriyor ama
yasayı deliyor. Karar verilmeden kod yazılmaz.

### Geri dönüşsüz komutlar

`rm`, `dd`, `chmod`, `kill` ve arkadaşları bugün **tamponlanmıyor** — öneri
ekrana yazılıyor, tampona konmuyor, çünkü tamponlanmış bir satır tek Enter'la
çalışır. Bu kural bir "tamponlama" aşaması olduğu için var.

Seste tamponlama aşaması yoksa kuralın tutunacağı yer de yok. Ses yolu bu
listeyi kendi başına yeniden karşılamak zorunda; kabuk zaten komutun başını
kendi listesine karşı denetliyor ([../CLASSIFIER.md](../CLASSIFIER.md)) ve o
denetim ses yolunda da hattın üstünde kalmalı.

## Çözülmemiş ikinci sıra sorular

**Tur alma.** Cümle nerede bitiyor. Sessizlik eşiği kısa olursa cümle
ortasında kesilir, uzun olursa kabuk yavaş hissedilir. Bu ölçülecek bir sayı,
tahmin edilecek bir sayı değil.

**Yanlış tetikleme.** Sürekli dinleme mi, uyandırma sözcüğü mü. Sürekli
dinleme odadaki her konuşmayı tanıma motorundan geçirir; yerel çalışsa bile
bu [../PRIVACY.md](../PRIVACY.md)'ye yazılacak bir davranış.

**Kesme.** Ctrl-C bugün sinyal işleyicisinden bir bayt yazıp beklemeyi
kesiyor. Ses yolunda kullanıcı konuşmanın ortasında vazgeçtiğinde karşılığı
ne.

**Dil.** Sınıflandırıcı bugün iki dilde girdi kabul ediyor. Tanıma motoruna
hangi dilin verileceği (ya da ikisinin birden) ayrı bir ayar.
