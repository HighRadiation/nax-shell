# Gizlilik: neyin gönderildiği

Bu kabuk her niyet isteğinde uzak bir servise veri yollar. Hangi verinin
yollandığı bir uygulama ayrıntısı değil, ürünün sözleşmesidir. Bu dosya o
sözleşmeyi yazar.

## Temel kural: temizleme kabuk tarafında yapılır

Sır temizleme, veri `naxd`'ye **verilmeden önce** C tarafında uygulanır.
Gerekçe basit: hatalı ya da ele geçirilmiş bir yardımcı süreç, hiç almadığı
veriyi sızdıramaz. Temizlemeyi gönderen tarafa koymak, güvenmek zorunda
olduğun kod miktarını küçültür.

## Varsayılan olarak gönderilen

- Çalışma dizini (ev dizini `~` ile kısaltılmış)
- Son çalıştırılan komutlar ve çıkış kodları (sınırlı sayıda)
- Git dalı ve dizinin temiz olup olmadığı
- Terminal genişliği, işletim sistemi, dil ayarı
- Geçmişte en sık kullanılan komut başları ve sayıları

Son madde modeli bu kullanıcıya uyarlar: hangi araçları kullandığını bilen bir
model daha isabetli komut üretir.

## Varsayılan olarak gönderilmeyen

- Dizin listesi
- Dosya içerikleri
- Çalıştırılan programların hata çıktısı
- Ortam değişkenlerinin **değerleri**

Model bunlara ihtiyaç duyduğunda ister; kabuk sabit bir listeden karşılar ve
istek başına en çok iki tur, sınırlı boyutta veri verir. Yani pahalı ve hassas
veri akışı açık, sayılabilir ve denetlenebilir kalır.

## Ortam değişkenleri

Yalnızca **isimler** gönderilir. Değeri gönderilen tek beyaz liste: çalışma
dizini, terminal türü, dil ayarı, kabuk adı, terminal genişliği.

Adında şu parçalardan biri geçen hiçbir değişkenin değeri hiçbir koşulda
gönderilmez: `KEY`, `TOKEN`, `SECRET`, `PASS`, `CRED`, `AUTH`, `PRIVATE`,
`COOKIE`, `SESSION`, `BEARER`, `AWS`, `STRIPE`, `DATABASE_URL`.

## Değer bazlı temizleme

Gönderilen her metin — komut satırları, geçmiş, istenen dosya parçaları —
şu kalıplar için taranır ve eşleşen kısım `[GIZLI:tur]` ile değiştirilir:

- API anahtarı biçimleri
- erişim jetonu önekleri
- bulut erişim anahtarı biçimleri
- imzalı jetonlar
- özel anahtar blokları
- adres içine gömülmüş kullanıcı adı ve parola
- komut satırındaki parola seçenekleri
- otuz iki karakterden uzun onaltılık ya da base64 görünümlü diziler

Yer tutucu bırakılması bilinçli: model orada bir sır olduğunu bilir, ne
olduğunu bilmez. Böylece "bu komut neden başarısız oldu" sorusunu yanıtlarken
eksik bilgiyle uydurmak zorunda kalmaz.

## Dosya bazlı ret

Şu kalıplara uyan dosyaların içeriği hiçbir koşulda istenmez ve
gönderilmez: ortam dosyaları, sertifika ve özel anahtar dosyaları, kimlik
dosyaları, paket yöneticisi ve ağ kimlik dosyaları, adında kimlik bilgisi
geçen dosyalar.

## Boşlukla başlayan satırlar

Bir boşlukla başlayan satırlar bağlama hiç girmez. Bu, geçmişe yazılmaması
istenen komutlar için yaygın bir alışkanlıktır; burada da aynı anlamı taşır ve
bedava gelir.

## Dizin bazlı tam kapatma

Belirli dizinler için AI tamamen kapatılabilir. Çalışma dizini o listeyle
eşleştiğinde hiçbir istek gönderilmez ve kullanıcıya tek satır bildirilir.

## Doğrulanabilirlik

`nax ctx` komutu, bir sonraki istekte gidecek **baytları aynen** basar.

Bu, gizlilik iddiasını denetlenebilir kılan tek özelliktir: belgeye güvenmek
zorunda değilsin, bakabilirsin. Aynı zamanda en iyi test kancasıdır —
testler sahte bir sır ekip `nax ctx` çıktısında görünmediğini doğrular.

## Yerel model

Gizlilik asıl önemliyse yerel model yolu verinin makineden hiç çıkmamasını
sağlar. Ayrıntısı [CONFIG.md](CONFIG.md) içinde; orada bu yolun dürüst
sınırları da yazılı.
