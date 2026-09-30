# Bulgular — ertelenmiş işler

Bu dosya "yapılacaklar" listesi değil. Yol üstünde fark edilen ama o oturumun
konusu olmadığı için ertelenen şeyleri kaydeder. Amacı işin unutulmamasını
sağlamak, kapsamı genişletmemek.

Bir oturum tek konuda kalır. Yolda bulunan her şey sorulmadan buraya tek satır
olarak, yeri `dosya:satır` biçiminde yazılır.

Kapanan madde silinmez: üstü çizilir ve nerede kapandığı yazılır. Böylece
listenin geçmişi de okunabilir kalır.

---

## Açık

| Tarih | Ne | Nerede | Neden şimdi değil |
|---|---|---|---|
| 2026-09-30 | **Koşul, yardımcı süreç aşaması için:** `SA_RESTART` olmadığı için yardımcı süreç beklemesindeki `poll` da `EINTR` döngüsüne sarılmak zorunda. Çalıştırıcı tarafı ödendi. | `src/naxd_client.c` (henüz yok) | Sarılacak kod henüz yok; bir hata değil, o kod yazılırken uyulacak bir koşul |
| 2026-09-30 | `<<` yönlendirmesi çalıştırılmıyor; anlaşılır mesaj verilip 1 dönüyor | `src/exec.c` (`unsupported`) | Gövdesini okumak için okuma döngüsünden ek satır almak gerekiyor; bu kabuğun girdi yolunu değiştiren ayrı bir iş |
| 2026-09-30 | Yerleşik komut yok; `exit` geçici olarak okuma döngüsünde ele alınıyor | `src/main.c` (`is_exit_request`) | Çalıştırıcının 3. parçası. `cd` ve `export` ana süreçte koşmak zorunda, o yüzden ayrı bir adım |
| 2026-09-30 | Prompt son komutun çıkış kodunu göstermiyor | `src/line.c` (`build_prompt`) | Bu aşamada hiçbir şey çalıştırılmadığı için kod her zaman sıfır; gösterge ölü kod olurdu |
| 2026-09-30 | Geçmiş dosyası sınırsız büyüyor; `history_truncate_file` çağrısı yok | `src/line.c` (`ln_hist_save`) | Tek satırlık iş ama bu oturumun konusu değil; geçmiş yönetimi sıradaki aşamada zaten elden geçecek |
| 2026-09-30 | Etkileşimsiz koşu da geçmiş dosyasını yüklüyor ve yazıyor. Görünür zararı yok (aynı içerik geri yazılıyor) ama gereksiz giriş/çıkış, ve iki eşzamanlı oturum birbirini ezebilir | `src/main.c` (`shell_init`, `shell_free`) | Aynı gerekçe: geçmiş yönetimi sıradaki aşamada toplu ele alınacak |
| 2026-09-30 | `~kullanıcı` biçimi desteklenmiyor; yalnızca `~` ve `~/...` açılıyor | `src/expand.c` (`expand_tilde`) | Kullanıcı veritabanı okumak gerekiyor (`getpwnam`); tek başına bu aşamanın konusu değil |
| 2026-09-30 | Alan ayırma yalnızca boşluk, sekme ve yeni satırda yapılıyor; `IFS` değişkeni okunmuyor | `src/expand.c` (`is_ifs`) | `IFS`'in boşluk olmayan ayırıcılarla semantiği ayrı bir iş; varsayılan davranış vakaların tamamını kapsıyor |
| 2026-09-30 | Yalnızca ortam değişkenleri okunuyor; dışa aktarılmamış kabuk değişkeni (`X=1` gibi) desteklenmiyor | `src/expand.c` (`getenv` çağrıları) | Kendi değişken deposu `export`/`unset` yerleşikleriyle birlikte gelecek; şimdi kurmak o işi ikiye bölerdi |
| 2026-09-30 | `$$`, `$!`, `$0` ve `${VAR:-x}` gibi parametre biçimleri desteklenmiyor. İlk üçü harfi harfine kalıyor, süslü biçimler **açık hata** veriyor | `src/expand.c` (`expand_dollar`, `expand_braced`) | Ad doğrulaması eklendi, yani sessizce yanlış cevap vermiyor; biçimlerin kendisi bu aşamanın kapsamı değil |
| 2026-09-30 | Ağaç kanonik çıktısı tırnak kipini göstermiyor: `echo a` ile `echo "a"` aynı metni üretir | `src/ast_dump.c` | Bilinçli — ayrıştırıcı tırnağa bakmaz, tırnak ayrıntısı sözcük ayırıcı testlerinin işi. Ama ileride tırnağa bağlı bir ayrıştırıcı hatası çıkarsa bu testlerle yakalanmaz |
| 2026-09-30 | Kanonik çıktı biçimi parça metnindeki `+`, `)` ve boşluğu kaçırmıyor; bu karakterleri içeren bir sözcük vakası okunurken belirsiz görünebilir | `src/lex_dump.c` (`dump_word`) | Mevcut 44 vakanın hiçbirinde belirsizlik yok; kaçış eklemek biçimi okunmaz yapardı. Gerçek bir belirsizlik çıkarsa o zaman eklenir |
| 2026-09-30 | Vaka dosyası satırları 4096 bayttan uzunsa test koşucusu sessizce bölerek okur | `test/test_lexer.c` (`LINE_MAX_LEN`) | En uzun vaka 60 bayt civarında; sınır aşılırsa fark edilir ve o zaman büyütülür |
| 2026-09-30 | `&&`, `||`, `;`, `&`, `(`, `)` sözcük ayırıcıda tanınıyor, ayrıştırıcı anlaşılır hata veriyor | `src/parser.c` (`is_unsupported`) | Bunları şimdi tanımak lexer'ı ikinci kez açmayı önlüyor; ayrıştırıcı desteği plandaki yerinde gelecek |
| 2026-09-30 | Sınıflandırıcıda bilinen açık: PATH'te karşılığı olan bir kelimeyle başlayan doğal dil cümlesi komut sanılabilir | `docs/CLASSIFIER.md` şekil vetosu bölümü | Şekil vetoları riski büyük ölçüde kapatıyor; kalan nadir durum için `nax <metin>` kaçış yolu var |

## Kapandı

| Tarih | Ne | Nerede kapandı |
|---|---|---|
| 2026-09-30 | ~~readline etkileşimsiz girdide okuduğu satırı yankılıyor, çıktıyı ikiye katlıyordu~~ | `src/line.c` — etkileşimsiz yol `getline` kullanır, readline yalnızca terminalde çalışır |
| 2026-09-30 | ~~Prompt'ta Ctrl-C kabuğu öldürüyordu (sinyal 2)~~ | `src/signal.c` — `rl_catch_signals` kapatıldı, işleyici yalnızca bayrak set ediyor, satır iptali `rl_event_hook` içinde normal bağlamda yapılıyor. Ctrl-C yarım satırı atar, `^C` basar, `$?`'yi 130 yapar; Ctrl-\ yok sayılır |
| 2026-09-30 | ~~Boru hattı ve yönlendirme çalıştırılmıyordu~~ | `src/exec.c`, `src/redir.c` — n aşamalı borular, dört yönlendirme türünden üçü (`<` `>` `>>`), sıra korunumu ve boru uçlarının kapatılması. Ölçüldü: 400 hat sonrası açık fd sayısı 3, hiç zombi yok |
| 2026-09-30 | ~~`R_OK` adlı enum sabiti `<unistd.h>`'nin `access()` makrosuyla çakıştı; ön işlemci her kullanımı `4` ile değiştirdi ve derleyici hiç uyarmadı. Dizin komut olarak çağrıldığında yanlış mesaj çıkıyordu~~ | `src/exec.h` — sabitler `RES_*` önekli oldu; `make check`'e bu sınıfı otomatik yakalayan bir denetim eklendi, denetim kasten çakışma sokularak doğrulandı |
| 2026-09-30 | ~~`make check`'in "sıfır sızıntı" iddiası kodun yarısını kapsıyordu: bütün testler boru ile besliyor, yani readline, geçmiş ve prompt üretimi hiç koşmuyordu~~ | `test/pty_drive.py` — sahte terminal üzerinden 12 vaka; `NAX_BIN` denetleyicili ikiliyi gösterdiğinde etkileşimli yol da sızıntı denetiminden geçer |
