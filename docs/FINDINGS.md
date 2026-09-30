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
| 2026-09-30 | **Koşul, sıradaki aşama için:** `SIGINT` işleyicisi `SA_RESTART` olmadan kuruldu, yani bloke eden çağrılar `EINTR` dönebilir. Çalıştırıcı geldiğinde `waitpid`, yardımcı süreç beklemesi geldiğinde `poll` **`EINTR` döngüsüne sarılmak zorunda.** Sarılmazsa Ctrl-C bir komut beklenirken bozuk davranır. | `src/signal.c` (gerekçe dosya başında) → `src/exec.c`, `src/naxd_client.c` | Sarılacak kod henüz yok; bu bir hata değil, o kod yazılırken uyulacak bir koşul |
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
| 2026-09-30 | ~~`make check`'in "sıfır sızıntı" iddiası kodun yarısını kapsıyordu: bütün testler boru ile besliyor, yani readline, geçmiş ve prompt üretimi hiç koşmuyordu~~ | `test/pty_drive.py` — sahte terminal üzerinden 12 vaka; `NAX_BIN` denetleyicili ikiliyi gösterdiğinde etkileşimli yol da sızıntı denetiminden geçer |
