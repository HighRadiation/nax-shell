# Temel denetimi

Yeni özellik eklenmeden önce temelin ölçümü. Tarih: 2026-10-05. Ölçülen
commit: `6b01f42`.

Biçim: her madde bir **kural**. Kural emir kipinde. Altında `Gerekçe` — ölçüm
ve `dosya:satır`. Yorum yok.

Durum: taban ölçümü bitti. On boyutluk kod taraması sürüyor; bu dosya
bitince genişleyecek.

## Ölçülen durum

| Ölçüm | Sonuç |
|---|---|
| `make` | Çıkış 0. 33 kaynak. **Sıfır uyarı.** |
| `make asan` | Çıkış 0. 33 kaynak. **Sıfır uyarı.** |
| `make check` | Çıkış 0. **1442 geçti, 0 patladı.** İki geçiş × 721. |
| `-Werror` | Varsayılan açık. `-Wall -Wextra -Wshadow -Wvla -Wstrict-prototypes -Wformat=2` |
| `"atlandi"` basılan test | 0 |
| Sessiz atlanan vaka | **8** (geçiş başına 4) |
| Ölçmeden yeşil yanan vaka | **1** (geçiş başına) |
| `TODO`/`FIXME`/`HACK` | 0 gerçek işaretleyici |
| Doğrudan birim testi olmayan kaynak | 23 / 34 |
| `docs/FINDINGS.md` | 27 açık, 13 kapandı |
| Depo büyüklüğü | 15.945 satır |

Taban sağlam. Aşağıdaki maddeler bu sağlamlığın **ölçülmeyen** kenarları.

## Test görünürlüğü

### `rules.test.1` — Vaka yönlendiricisi `else` dalı taşımalı. Bilinmeyen önek `report_fail` üretmeli.

**Gerekçe:** `test/unit_wire.py:run_case` `else`'siz. `test/cases/proto.tsv`
40 vaka taşıyor, koşucu 36 bildiriyor. Dört `C:` vakası ne geçti ne patladı:
`proto.tsv:97`, `:99`, `:101`, `:105`. C ikizi `test/test_proto.c:286`
bilinmeyen öneke `report_fail` veriyor. Kural `test/harness.c:7-12`'de zaten
yazılı — "sessiz atlama yeşil bir suite gösterir ama hiçbir şey ölçmez" —
Python tarafına uygulanmamış.

Kapsam kaybı yok; dördü C tarafında gerçekten koşuyor. Kayıp olan görünürlük.

### `rules.test.2` — Atlanan vaka sayılarak atlanmalı. Özet satırında ayrı sütun olmalı.

**Gerekçe:** Özet bugün yalnız "N geçti, M patladı" basıyor. Atlanan için yer
yok. 40'lık tablo 36 bildirdiğinde kimse farketmiyor.

### `rules.test.3` — Beklenen değer `NULL` olsa bile üretici işlev çağrılmalı.

**Gerekçe:** `test/unit_wire.py:case_build`, `want=="NULL"` ise `wire.build`'i
hiç çağırmadan `ok()` veriyor. Vaka: `B:WAT|id=1 -> NULL`. C tarafı
`test/test_proto.c:154-159` aynı vakayı gerçekten ölçüyor.

### `rules.test.4` — Tablo satır sayısı, koşucunun bildirdiği sayıya eşit olmalı. Eşitsizlik ekrana yazılmalı.

**Gerekçe:** `proto.tsv` 40 → `test_proto.c` 40, `unit_wire.py` 36. Fark bugün
yalnız elle sayılarak bulunuyor.

### `rules.test.5` — `NAX_UNITS` boşken `run.sh` sessizce geçmemeli.

**Gerekçe:** `test/run.sh:484` ve `:494` `[ -x "$unit" ]` arıyor. İkili yoksa
hiçbir şey yazmadan geçiyor. Elle `./test/run.sh` koşulduğunda 11 C birim
takımı — 443 vaka — sessizce yok oluyor, betik yine sıfır dönüyor.

`make check` bunu koruyor: `make` ikiliyi kuramazsa durur. Koruma elle koşuda
yok.

### `rules.test.6` — Python birim testi çalıştırma bitiyle seçilmemeli.

**Gerekçe:** `test/run.sh:494` `[ -x ]` arıyor. Git modları bugün doğru
(`unit_*.py` 100755). Bit kaybı — zip, bozuk checkout, umask — iki Python
takımını (66 vaka) sessizce siler ve testler yine yeşil yanar.

### `rules.test.7` — `NAX_BIN`'e bağlı olmayan test iki geçişte koşmamalı.

**Gerekçe:** `naxd_py` 30 + `wire` 36 = 66 vaka, `make check` içinde birebir
iki kez koşuyor. 1442'nin 66'sı çift sayım. Bilgi eklemiyor.

## Kapsam

### `rules.cover.1` — 500 satırı geçen kaynak, doğrudan birim testi olmadan kalmamalı.

**Gerekçe:** `src/ai/bridge.c` 561 satır — depodaki en büyük dosya.
`test/test_bridge.c` yok. `docs/FINDINGS.md`'nin 23 ve 26 numaralı açık
maddeleri (yardımcı süreç yolu, `/proc/self/exe`) tam bu dosyada oturuyor.

### `rules.cover.2` — Test dosyası adı, test ettiği kaynağın adıyla eşleşmeli.

**Gerekçe:** `src/ai/classifier.c` ↔ `test/test_classify.c` eşleşmiyor.
Otomatik kapsam taraması bu dosyayı yanlış olarak "testsiz" sayıyor. Ad
eşleşmesi norm olmadan kapsam sorusu her seferinde elle cevaplanıyor.

### `rules.cover.3` — "Doğrudan test dosyası yok" raporlanırken "ölçülmüyor" denmemeli. Dolaylı kapsam ayrı gösterilmeli.

**Gerekçe:** `veto.c`, `b64.c`, `fix.c`, `heredoc.c` doğrudan dosyası olmadan
`classify.tsv`, `proto.tsv` ve 170 boru + 42 pty vakası üzerinden gerçekten
koşuyor. Ham "testsiz 5320 satır" değeri yanlış alarm veriyor.

## Derleme

### `rules.build.1` — `-Werror` varsayılan kalmalı. `W=0` hiçbir betiğe girmemeli.

**Gerekçe:** `make` ve `make asan` bugün 33+33 dosyada sıfır uyarı üretiyor.
Korunması bedava, geri kazanılması değil.

### `rules.build.2` — Derleme bayrakları tek yerden gelmeli. Link satırına ayrı `-I` yazılmamalı.

**Gerekçe:** `Makefile`'da ASAN link kuralı `-Isrc` yazıyor, derleme kuralı
`$(INC)` (dört yol) kullanıyor. Bugün zararsız — link aşaması başlık okumuyor
— ama iki ayrı doğruluk kaynağı.

## Doküman ve depo

### `rules.doc.1` — Bir madde "testle gözlemlenemiyor" diye kaydediliyorsa, hangi mutasyonun kaçtığı da yazılmalı.

**Gerekçe:** `docs/FINDINGS.md`'nin 19, 20, 21, 22 numaralı maddeleri bu
sınıfta ve gerekçeleri yazılı. Çalışan bir norm; kural haline gelmesi gereken
şey bunun zorunlu olması.

### `rules.doc.2` — Kodda `TODO`/`FIXME`/`XXX`/`HACK` kullanılmamalı. Ertelenen iş `FINDINGS.md`'ye `dosya:satır` ile yazılmalı.

**Gerekçe:** Tarama sıfır gerçek işaretleyici buldu; 27 açık madde
`FINDINGS.md`'de duruyor. Norm fiilen uygulanıyor, yazılı değil.

Tarama deseni düzeltilmeli: `XXX` yerine `\bXXX\b`. Bugünkü desen üç `mkstemp`
şablonuna takılıyor (`test/test_classify.c:51`, `test/test_naxd.c:491`,
`test/test_lineio.c:156`).

### `rules.repo.1` — Sır içeren yapılandırma asla takip edilmemeli.

**Gerekçe:** `nax.conf` mod 600, git'te yok, `.gitignore:13` kapsıyor.
`nax.conf.example` takip ediliyor. Doğru durum; kayıt altına alınıyor.
