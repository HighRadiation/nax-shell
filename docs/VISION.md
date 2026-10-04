# Hedef

Bu dosya **nereye gittiğimizi** tutar. Nerede kaldığımız ve sırada ne olduğu
[ROADMAP.md](ROADMAP.md)'de; bugünkü mimari
[ARCHITECTURE.md](ARCHITECTURE.md)'de; ertelenen işler
[FINDINGS.md](FINDINGS.md)'de.

Buradaki hiçbir şey ölçülmedi. Yol haritasındaki maddelerin aksine bunlar
plan, kapanmış iş değil. Bir madde ölçülüp koda girdiğinde yeri burası değil,
ROADMAP ve ARCHITECTURE olur.

Bu dosya yönü tutuyor. Parçaların ayrıntısı [v0.2.0/](v0.2.0/README.md)
klasöründe, her özellik ayrı dosyada:
[ses](v0.2.0/VOICE.md), [bellek](v0.2.0/MEMORY.md),
[görsel](v0.2.0/VISUAL.md), [işletim sistemi](v0.2.0/OS.md),
[platformlar](v0.2.0/PLATFORMS.md).

## Nihai hedef

nax bir **AI-OS** olacak: yerel bir kişisel asistan, taşıyıcısı da kabuğun
kendisi. Mini kabuk nihai ürün değil, asistanın gövdesi.

Bugünkü mimari bunun için zaten doğru yerde duruyor. AI, terminalin *yanında*
değil kabuğun *dilinin içinde*; `naxd` ayrı bir süreç ve ölürse kabuk yaşıyor;
bağlam (dizin, son komutlar, çıkış kodları, git dalı) hiç sorulmadan biliniyor.
Asistan bunların üstüne gelir, yanlarına değil.

## Sıra

1. **Kabuk çekirdeği kusursuz çalışsın.** Önce bu. Yarısı bitmiş bir kabuğun
   üstüne ses katmanı koymak iki yarım iş demek.
2. **Ses katmanı.** Sesle dinleyip metinle cevap veren sistem.
3. **Kalıcı bellek.** nax kullanıcının kim olduğunu bilsin — oturum bağlamı
   değil, oturumlar arası kalan bilgi.
4. **İşletim sistemi.** Zamanla.

Sıra bozulmaz. Her adım öncekinin üstünde duruyor, yanında değil.

## "İşletim sistemi" iki ayrı proje demek

**Linux tabanlı AI-OS — ulaşılabilir.** Çekirdek Linux kalır; nax init ya da
giriş kabuğu olur ve minimal bir sistemin tüm kullanıcı alanını tutar. Android
Linux'tur, ChromeOS Linux'tur; kimse onlara "işletim sistemi değil" demiyor.
Mevcut kod bunun çekirdeği.

**Kendi çekirdek — uzak.** Kendi zamanlayıcı, sürücüler, bellek yönetimi,
dosya sistemi. Asıl engel modelin nerede çalıştığı: ağ yolu kendi TCP/IP ve
TLS yığınını ister, yerel çıkarım GPU/NPU sürücüsü ister. `naxd` hiç taşınmaz
(Python, HTTPS); `src/parse/`, `src/exec/` ve `glob.c` gibi saf C mantığı
kısmen taşınır.

Hangi yol seçilirse seçilsin bugünün işi aynı: kabuk çekirdeği. Ayrım
sonraki adımda, bugün değil.

### Linus benzetmesinin sınırı

"Linus da bir terminal yazarak başladı, zamanla Linux'a döndü" doğru, ama
aradaki ayrım bu projeyi birebir ilgilendiriyor.

Linus'un 1991'deki terminal emülatörü **bare metal**'di. Altında işletim
sistemi yoktu: kendi görev değiştiricisi, kendi klavye, seri port ve ekran
sürücüleri vardı, floppy'den boot ediyordu. Yani ilk gününden beri çekirdek
koduydu. Dosya indirmek isteyince disk sürücüsü ve dosya sistemi yazmak
zorunda kaldı — çekirdek o an doğdu.

nax ise bir kullanıcı alanı programı: `readline`, `fork`, `execve`, `dup2`,
`waitpid`. Her satırda çekirdeğe iş veriyor. Büyüdükçe bir çekirdeğe
yakınsamaz, zengin bir kullanıcı alanına yakınsar.

Aynı kelime, farklı madde. Linus'un 1991'de ihtiyacı üç sürücüydü; bir
AI-OS'un ihtiyacı bir TLS yığını ya da bir GPU sürücüsü.

## Ses katmanı

Kalıp zaten var. `naxd` modelin kendisi: ayrı süreç, satır tabanlı protokol,
ölürse kabuk yaşar. Bir konuşma tanıma süreci aynı kapıdan geçer — `whisper.cpp`
bu kalıba uyuyor (C++, CPU'da çalışır, kurulum istemez).

**Açık soru, çözülmeden ses yazılmaz: birinci yasanın ses karşılığı yok.**

"Hiçbir komut sen görmeden çalışmaz" bir klavye ve ekran yasası. Komutu
çalıştıran şey kullanıcının Enter'a basması; tampon da bunun için var. Seste
Enter yok.

Bu cevaplanmadan ses yolu eklenirse yasa sessizce iptal olur — üstelik kodda
hiçbir şey kırılmadan, yani test de yakalamaz. Ürünün en güçlü sözü o yasa.
Sorulacak soru şu: **Enter'ın yerine ne geliyor?**

Geri dönüşsüz komutların tamponlanmaması kuralı (`rm`, `dd`, `chmod`, `kill`)
seste de ayrıca düşünülmek zorunda; orada "tamponlama" diye bir aşama yoksa
kuralın tutunacağı yer de yok.

## Görsel katman

Düşünülen şey Jarvis'e benzer bir görsel model: nax çalıştığında terminal
ekranı yerine onun açılması. Model Blender'da üretilecek.

**Blender model üretir, modeli koşturmaz.** Blender bir içerik üretim
uygulaması, bir arayüz araç takımı değil: açılışı saniyelerle ölçülür, yüzlerce
MB ister, GPU ister. `bpy` yalnızca Blender'ın gömülü Python'unda çalışır ve
uygulama arayüzü için bir olay/pencere katmanı yok.

Daha önemlisi **ikinci yasa kırılır**. Görsel katman ekranın kendisi olursa
Blender çöktüğünde kabuk da gider. "AI kırılırsa kabuk çalışır" sözü
"Blender kırılırsa kabuk gider"e dönüşür.

Ayrım şöyle kurulur:

- **Üretim:** Blender. Model orada yapılır, glTF olarak ihraç edilir.
- **Koşturma:** ayrı ve hafif bir motor — Godot, raylib ya da doğrudan OpenGL.
- **Yetki:** terminal gerçek arayüz kalır, görsel onun bir **görünümü** olur.

Jarvis filmde de sistemin kendisi değildi, sistemin yüzüydü.

## Platformlar

**Linux öncelikli.** Bugün desteklenen tek platform bu ve öyle kalıyor.

**macOS ikinci.** Dört somut engel [FINDINGS.md](FINDINGS.md)'de yazılı:
yardımcı sürecin yolu (`/proc/self/exe`), readline başlıklarının sabit yolu,
çevrimdışı tablodaki GNU'ya özgü komutlar (`free -h`, `ss -tlnp`, `sort -rh`,
`find -newermt`) ve testlerin `/proc` ile LeakSanitizer'a bağlı olması. İlk
ikisi mekanik, üçüncüsü tasarım kararı istiyor.

**Windows ayrı bir mesele.** POSIX çekirdeği altı dosyada toplanıyor:
`src/core/signal.c`, `src/exec/exec.c`, `src/exec/stage.c`, `src/exec/redir.c`,
`src/exec/exec.h`, `src/ai/naxd.c`. `fork`, `execve`, `dup2`, `waitpid` ve
`sigaction`'ın Windows'ta karşılığı yok; `CreateProcess`, handle devralma ve
job object demek. `readline` da yok.

İki seçenek var ve ikisi aynı şey değil: WSL bedava ama orada çalışan şey
zaten Linux. Gerçek Windows desteği `src/exec/` arkasına bir platform katmanı
istiyor. Bu bedel şimdi ödenmiyor — ama `src/exec/` zaten bir iş için
açıldığında dikişin nereden geçeceği o anda belirlenir.
