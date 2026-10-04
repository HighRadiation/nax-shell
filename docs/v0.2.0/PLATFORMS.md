# Platformlar: macOS ve Windows

**Hedef:** Linux öncelikli kalsın, sonra macOS, sonra Windows.

Durum: Linux çalışıyor. macOS'ta derlenmiyor ve engelleri ölçülmüş durumda.
Windows için hiçbir şey yapılmadı.

## Linux

Bugün desteklenen tek platform ve öncelik sırası değişmiyor. Yeni özellikler
önce burada çalışır.

## macOS — dört somut engel

[../FINDINGS.md](../FINDINGS.md)'de 2026-10-02 tarihiyle kayıtlı. `make`
macOS'ta kendi anlaşılır mesajıyla duruyor, sessizce bozuk bir ikili
üretmiyor.

| Engel | Yer | Niteliği |
|---|---|---|
| Yardımcı sürecin yolu `/proc/self/exe` ile bulunuyor, macOS'ta `/proc` yok | `src/ai/bridge.c:82` | Mekanik: `_NSGetExecutablePath()` |
| `Makefile` readline başlığının yolunu sabit yazıyor; macOS libedit getiriyor, gerçek readline Homebrew'dan geliyor | `Makefile` (`READLINE_H`) | Mekanik: `brew --prefix readline` |
| Çevrimdışı tabloda `free -h`, `ss -tlnp`, `sort -rh`, `find -newermt` — BSD'de yok | `src/ai/offline.c` | **Tasarım kararı** |
| Testler `/proc/self/status` okuyor; LeakSanitizer Darwin'de desteklenmiyor | `test/run.sh` | Mekanik: platform koşulu |

Üçüncüsü iş değil, karar: çevrimdışı tabloya işletim sistemine göre satır mı
konacak, yoksa taşınabilir komut mu yazılacak. İlki tabloyu ikiye katlıyor,
ikincisi bazı komutları kötüleştiriyor (`ss -tlnp` yerine taşınabilir bir
karşılık aynı bilgiyi vermiyor). Karar verilmeden kod yazılmaz.

Birinci engelin ayrıca bir tarihi var: yardımcı sürecin yolu düzeltilmeden
önce `cd` sonrası AI kayboluyordu (`d9fce59`). macOS'ta `/proc` olmadığı için
aynı hata orada geri geliyor.

## Windows — ayrı bir mesele

POSIX bağımlılığı altı dosyada toplanıyor:

- `src/core/signal.c`
- `src/exec/exec.c`
- `src/exec/stage.c`
- `src/exec/redir.c`
- `src/exec/exec.h`
- `src/ai/naxd.c`

Buradaki `fork`, `execve`, `dup2`, `waitpid` ve `sigaction`'ın Windows'ta
karşılığı yok. Karşılığı `CreateProcess`, handle devralma ve job object
demek — eşleme değil, yeniden yazım. `readline` da yok; `rl_insert_text` ile
kurulan tampon davranışının (yani birinci yasanın mekanizması) yeni bir
temeli gerekir.

İki seçenek var ve ikisi aynı şey değil:

**WSL.** Bedava. Ama orada çalışan şey zaten Linux; "Windows desteği" değil,
"Windows'ta Linux" oluyor. Kullanıcı için yeterli olabilir, teknik olarak bir
iş değil.

**Gerçek Windows.** `src/exec/` arkasına bir platform katmanı istiyor. Bu
bedel şimdi ödenmiyor.

Yalnız bir şey şimdiden belirlenebilir: `src/exec/` zaten başka bir iş için
açıldığında **dikişin nereden geçeceği** o anda seçilir. Katmanı sonradan
kazımak, bugün doğru yerden ayırmaktan pahalı.

## Sıra dışı not

Bu dosyadaki işlerin hiçbiri yeni özellik değil; taşıma işi. Ses, bellek ve
görsel katman sırada bunların önünde — bir özelliği üç platformda birden
yazmak, bir platformda bitirmekten yavaş.
