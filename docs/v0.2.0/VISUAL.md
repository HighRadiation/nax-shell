# Görsel katman

**Hedef:** Jarvis'e benzer bir görsel model. nax çalıştığında çıplak terminal
ekranı yerine onun açılması. Model Blender'da üretilecek.

Durum: planlandı, ölçülmedi. Kod yok.

## Blender model üretir, modeli koşturmaz

Blender bir **içerik üretim uygulaması**, bir arayüz araç takımı değil.
Çalışma zamanı arayüzü olarak kullanmanın bedeli ölçülebilir ve ağır:

- Açılışı saniyelerle ölçülür. Kabuk açılışı milisaniyelerle ölçülüyor.
- Yüzlerce MB bellek ve bir GPU ister. Kabuğun bugünkü maliyeti bunun yanında
  yok sayılabilir.
- `bpy` yalnızca Blender'ın gömülü Python'unda çalışır. Yani arayüzü süren kod
  Blender'ın içinde yaşamak zorunda kalır — bağımlılık yönü tersine döner.
- Uygulama arayüzü için bir olay/pencere katmanı yok. Blender'ın kendi
  arayüzü var ve o arayüz başka bir şey için tasarlandı.

## Daha önemlisi: ikinci yasa kırılır

"AI kırılırsa kabuk çalışır" bu deponun iki yasasından biri. Ağ gitse, anahtar
olmasa, yardımcı süreç ölse kabuk bir uyarı satırı basıp düz kabuk olarak
devam ediyor.

Görsel katman **ekranın kendisi** olursa bu söz tersine döner: Blender
çöktüğünde kullanıcının kabuğu da gider. Yasa "Blender kırılırsa kabuk gider"e
iner ve bu bir gerileme olur.

Kabuk bir kabuk; görsel katman onun aşamalarından biri olabilir, taşıyıcısı
olamaz.

## Üçlü ayrım

- **Üretim: Blender.** Model orada yapılır, glTF olarak ihraç edilir. Blender
  çalışma zamanında hiç bulunmaz.
- **Koşturma: ayrı ve hafif bir motor.** Godot, raylib ya da doğrudan
  OpenGL. Seçim ölçütü: tek ikili olarak dağıtılabilmesi, GPU yoksa
  çalışabilmesi (ya da zarifçe kapanması), C'den sürülebilmesi.
- **Yetki: terminalde.** Terminal gerçek arayüz kalır, görsel onun bir
  **görünümü** olur. Tersi değil.

Jarvis filmde de sistemin kendisi değildi, sistemin yüzüydü.

## Süreç modeli

Kalıp yine `naxd`'nin kalıbı: ayrı süreç, satır tabanlı protokol, ölümü
tolere edilen. Görsel süreç çökerse kabuk bir uyarı satırı basar ve çıplak
terminal olarak devam eder — ses ve bellek katmanları için geçerli olan aynı
sözleşme.

Bu, [../PROTOCOL.md](../PROTOCOL.md)'nin ikinci (ses ile birlikte üçüncü)
tüketicisi demek. Protokolün bugün tek tüketicisi var ve genelleşmesi
gerekip gerekmediği bu üç katman birlikte düşünülerek karara bağlanmalı —
tek tek eklenirse üç ayrı yarım protokol çıkar.

## Çözülmemiş sorular

**Kim kimi başlatır.** Kabuk görseli mi açar, görsel kabuğu mu gömer.
İkinci yasa birinciyi zorunlu kılıyor: kabuk üstte kalmalı.

**Terminal nerede.** Görsel açıkken komut yazılan yer nerede — görselin içinde
bir panel mi, ayrı bir pencere mi, yoksa görsel yalnızca ikinci bir ekranda
mı duruyor. Bu, özelliğin ne kadar iş olduğunu belirleyen soru.

**Model ne gösterir.** Dekoratif bir yüz mü, yoksa durumu gösteren bir şey mi
— dinliyor, düşünüyor, hata var, komut bekliyor. İkincisi faydalı, birincisi
yalnızca maliyet.

**GPU yokken.** Sunucuda, SSH üstünde, konsol üstünde çalışırken ne olur.
Cevap "kabuk normal çalışır" olmak zorunda, yoksa nax taşınabilirliğini
görsel bir süs için kaybeder.
