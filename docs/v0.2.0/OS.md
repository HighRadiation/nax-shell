# İşletim sistemine dönüşme

**Hedef:** nax zamanla bir AI-OS olsun.

Durum: yön belli, iş tanımı belli değil. Bu dosya kararı vermiyor; kararın
iki seçenekten biri olduğunu ve ikisinin aynı proje olmadığını kayda geçiriyor.

## "İşletim sistemi" iki ayrı proje demek

### A — Linux tabanlı AI-OS. Ulaşılabilir.

Çekirdek Linux kalır. nax minimal bir sistemin tüm kullanıcı alanını tutar:
giriş kabuğu, hatta `init`. Android Linux'tur, ChromeOS Linux'tur; ikisine de
kimse "işletim sistemi değil" demiyor.

Mevcut ~10.000 satır C bunun çekirdeği. Yeniden yazılacak bir şey yok,
üstüne eklenecek şeyler var:

- **`init` görevleri.** PID 1 olmak bugün yapılmayan bir iş: sahipsiz
  süreçlerin toplanması, dosya sistemlerinin bağlanması, servislerin
  başlatılması, kapanış sırası. Kabuk bugün tek bir kullanıcı oturumu
  varsayıyor.
- **İş denetimi.** `&`, `fg`, `bg`, süreç grupları, `tcsetpgrp`. Bir sistemin
  tek kabuğu olacaksa bunlar seçenek değil.
- **Kullanıcı ve oturum.** Giriş, yetki, birden çok terminal.

Bunların hiçbiri araştırma işi değil; bilinen iş. Sıra meselesi.

### B — Kendi çekirdek. Uzak.

Kendi zamanlayıcı, sürücüler, bellek yönetimi, dosya sistemi.

Asıl engel sanıldığı yerde değil. Engel **modelin nerede çalıştığı**:

- **Ağ yolu** kendi TCP/IP ve TLS yığınını ister. Bir AI-OS'un ilk
  ihtiyacı budur ve tek başına bir proje.
- **Yerel çıkarım** GPU ya da NPU sürücüsü ister; CPU'da çıkarım ise bellek
  yönetimi ve kayan nokta performansı demek.

Taşınırlık dökümü: `naxd` hiç taşınmaz (Python yorumlayıcısı ve HTTPS
yığını ister). `src/parse/`, `src/exec/` ve `src/parse/glob.c` gibi saf C
mantığı kısmen taşınır. `readline`, `/proc` okumaları ve süreç modeli
taşınmaz.

## Linus benzetmesinin sınırı

"Linus da bir terminal yazarak başladı, zamanla Linux'a döndü" doğru. Ama
aradaki ayrım tam olarak bu projeyi ilgilendiriyor.

Linus'un 1991'deki terminal emülatörü **bare metal**'di. Altında işletim
sistemi yoktu: kendi görev değiştiricisi vardı, kendi klavye, seri port ve
ekran sürücüleri vardı, floppy'den boot ediyordu. Yani ilk gününden beri
çekirdek koduydu. Dosya indirmek isteyince disk sürücüsü ve dosya sistemi
yazmak zorunda kaldı — çekirdek o anda doğdu.

nax ise bir **kullanıcı alanı programı**: `readline`, `fork`, `execve`,
`dup2`, `waitpid`. Her satırda çekirdeğe iş veriyor. Büyüdükçe bir çekirdeğe
yakınsamaz; zengin bir kullanıcı alanına yakınsar.

Aynı kelime, farklı madde. Linus'un 1991'de ihtiyacı üç sürücüydü; bir
AI-OS'un ihtiyacı bir TLS yığını ya da bir GPU sürücüsü.

Bu benzetmeyi reddetmek hedefi küçültmüyor — yalnızca (A)'nın gerçek ve
ulaşılabilir, (B)'nin ayrı bir proje olduğunu söylüyor.

## Bugün ne değişiyor

Hiçbir şey. Hangi yol seçilirse seçilsin bugünün işi kabuk çekirdeği.

Ayrım sonraki adımda önemli olacak: (A) ise `init` ve iş denetimi sıraya
girer; (B) ise kabuğu cilalamak yanlış sıra olur — önce önyükleme gelir.
Karar verilmesi gereken an, kabuk çekirdeği kapandığı an.
