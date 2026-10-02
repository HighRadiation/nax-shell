# Yapılandırma

## Dosya

Yapılandırma proje kökündeki `nax.conf` dosyasındadır. Bu dosya `.gitignore`
içindedir ve asla depoya girmez; takip edilen sürüm `nax.conf.example`.

```
cp nax.conf.example nax.conf
chmod 600 nax.conf
```

İki tasarım kararı:

- Dosyayı **yalnızca `naxd` okur**, kabuk (C tarafı) okumaz. Yapılandırma tek
  bir yerde kalır, C tarafında ayrıştırıcı yazmaya gerek olmaz.
- `naxd` dosyanın değişme zamanını her istekte kontrol eder. Kaydettiğin an
  yeni ayar geçerli olur; **kabuğu yeniden başlatmak gerekmez.**

## Sağlayıcılar

Üç farklı istek biçimi vardır, ama pratikte hepsi iki adaptörle karşılanır:

| Adaptör | Kapsadığı | Biçim |
|---|---|---|
| uyumlu | Groq, OpenRouter, OpenAI, Gemini uyumluluk ucu, NVIDIA, yerel sunucular | `/v1/chat/completions`, `Bearer` başlığı |
| anthropic | Claude API | `/v1/messages`, ayrı başlıklar, içerik blokları |
| komut | Abonelikle çalışan yerel komut satırı araçları | alt süreç, çıktısı okunur |

`provider` alanı hangi adaptörün kullanılacağını belirler. İlk gruptaki
sağlayıcılar arasında geçiş yapmak yalnızca `base_url`, `api_key` ve model
adlarını değiştirmek demektir — kod değişmez.

**Varsayılan Groq.** Gerekçe düşük gecikme: bir kabukta yanıt süresi
kalitenin parçasıdır, iki saniye ile üç yüz milisaniye arasındaki fark
"sihirli" ile "sinir bozucu" arasındaki farktır. Esneklik isteniyorsa
OpenRouter eşdeğer bir başlangıçtır: tek anahtarla çok sayıda modele erişir ve
aynı kod yolunu kullanır.

## Bulut varsayılan, yerel opsiyon

Bu sıra bilinçlidir. Bulut her makinede çalışır, kurulum istemez ve hızlıdır.
Yerel model gizlilik ve bağımsızlık için vardır; temel yol değildir.

Ölçülebilir hız sıralaması: **bulut > yerel GPU > yerel işlemci.** Özel
donanım üzerinde koşan bir bulut sağlayıcısı, masaüstü bir ekran kartından
hızlıdır. Yerel modelin kazandığı şey hız değil, verinin makineden hiç
çıkmamasıdır. Gerekçeyi oraya koymak gerekir.

Ekran kartı olmayan kullanıcı cezalandırılmış olmaz: bulut yolu onun için de
hızlı çalışır ve ücretsiz katmanlar bu iş yüküne yeter.

## Yerel model aynı makinede olmak zorunda değil

`local_url` bir adrestir, `127.0.0.1` olması şart değil. Kabuk ekran kartı
olmayan bir sunucuda koşarken, güçlü makinede yerel model sunucusu çalışır ve
buraya o makinenin adresi yazılır:

```
use_local = true
local_url = http://<guclu-makinenin-adresi>:11434/v1
```

Bu kurulumun dürüst bir sınırı var: artık "çevrimdışı" değil, "kendi ağında"
çalışıyorsun. Makineler arası bağlantı bir aktarıcı üzerinden kuruluyorsa
internet gittiğinde o da gider.

## Çevrimdışı durumun asıl cevabı

Ekran kartı olmayan ve ağı olmayan bir makinede küçük bir dil modelinin istem
işleme süresi kabuğu kullanılamaz hale getirir: üretim hızı değil, istemi
*okuma* süresi sorun olur. Sekiz yüz jetonluk bir bağlamı zayıf bir işlemcide
okumak onlarca saniye sürer ve bir kabukta otuz saniye "özellik yok" demektir.

Bu yüzden çevrimdışı değerin büyük kısmı dil modelinden değil, zaten var olan
deterministik katmandan gelir ve sıfır gecikme, sıfır bellek tutar:

- yazım hatası düzeltme
- PATH çözümleme
- sınıflandırıcı
- elle yazılmış, sık kullanılan niyetlerden oluşan bir eşleme tablosu

Yerel dil modeli bu tablonun **üstünde** opsiyonel bir katmandır, altında
değil.

## Düşme kuralı

`local_fallback = true` iken bulut çağrısı iki kez üst üste başarısız olursa
yerel modele geçilir.

Tetikleyicinin "internet var mı" olmaması bilinçli. Gerçek hayatta internetin
gitmesi yılda birkaç kez olur; API arızası, istek sınırına takılma, kota
bitmesi ve ağın yavaşlaması ayda birkaç kez olur. Mekanizma aynı olduğu için
doğru tetikleyiciyi seçmek bedava bir kazanç.

## Alanlar

| Alan | Anlamı |
|---|---|
| `provider` | Hangi adaptör ve hangi servis |
| `base_url` | Bulut sağlayıcının adresi |
| `api_key` | Bulut sağlayıcının anahtarı; boşsa AI kapalı kalır, kabuk çalışır |
| `model_intent` | Niyeti komuta çeviren model — küçük ve hızlı olmalı |
| `model_explain` | Hatayı açıklayan model — akıl gerektirir, büyük olabilir |
| `use_local` | `true` ise yerel modele geçer |
| `local_url` | Yerel model sunucusunun adresi |
| `local_model` | Yerel model adı |
| `local_fallback` | Bulut iki kez patlarsa yerele düş |
| `timeout` | Bir isteğin sert kesilme süresi (saniye) |
| `spinner` | Çalışma göstergesinin belirme süresi (saniye) |

### Görev başına iki model

Türkçe bir cümleyi tek bir kabuk satırına çevirmek dar bir iştir; küçük ve
hızlı bir model burada daha iyi sonuç verir, çünkü gecikme kalitenin
parçasıdır. Hata açıklamak ise akıl gerektirir ve büyük bir modelin bedelini
hak eder. Tek bir model alanı bu iki farklı işi aynı kefeye koyardı.

Bir not: küçük modeller Türkçede İngilizceden belirgin biçimde zayıftır.
Niyetler Türkçe yazılacağı için model seçerken bu etken göz önünde
bulundurulmalıdır.

## İki ortam değişkeni

| Değişken | Ne işe yarar |
|---|---|
| `NAX_CONF` | Yapılandırma dosyasının yolunu değiştirir. Testler bunu kullanıyor; birden fazla kurulumu yan yana denemek için de işe yarar |
| `NAX_NAXD` | Yardımcı süreci başlatacak komutu değiştirir. Varsayılan `python3 naxd/naxd.py`. Testler bunu kasten kötü davranan bir taklide yöneltiyor |

İkisi de sıradan ayar değil, **yolu değiştiren** anahtarlar. Bu yüzden
`nax.conf` içinde değiller: yapılandırma dosyasının yerini yapılandırma
dosyasında belirtmek olmaz, ve yardımcı sürecin kendisi o dosyayı okuyan taraf.

## Süreler el sıkışmada gelir

`timeout` ve `spinner` bu dosyada durur ama dosyayı yalnızca `naxd` okur.
Beklemeyi ise kabuk yapar. Bu yüzden `naxd` iki değeri `READY` kaydıyla
kabuğa bildirir — C tarafında ayrıştırıcı yazmak gerekmez ve tek kaynak
korunur. Kabuk gelen değeri makul aralığa kırpar; karşı taraf sıfır ya da
saçma bir süre bildirirse kabuk ya hiç beklemez ya sonsuza kadar bekler.

## Anahtar yoksa

`api_key` boş olduğunda `naxd` bunu açılışta bildirir, kabuk oturumda bir kez
tek satır uyarı basar ve düz kabuk olarak tam işlevle çalışır. Anahtarsız
durum bir hata hali değil, desteklenen bir çalışma biçimidir.
