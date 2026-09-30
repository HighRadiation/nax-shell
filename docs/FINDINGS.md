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
| 2026-09-30 | Prompt'ta Ctrl-C kabuğu öldürüyor (sinyal 2). readline'ın varsayılan davranışı; olması gereken satırı temizleyip yeni prompt vermek ve `$?`'yi 130 yapmak. Ölçüldü ve doğrulandı. | `src/line.c` (`read_interactive`) | Sinyal yönetimi sıradaki kilometre taşının işi; oraya kendi modülü ile gelecek |
| 2026-09-30 | Prompt son komutun çıkış kodunu göstermiyor | `src/line.c:build_prompt` | Bu aşamada hiçbir şey çalıştırılmadığı için kod her zaman sıfır; gösterge ölü kod olurdu |
| 2026-09-30 | Sınıflandırıcıda bilinen açık: PATH'te karşılığı olan bir kelimeyle başlayan doğal dil cümlesi komut sanılabilir | `docs/CLASSIFIER.md` şekil vetosu bölümü | Şekil vetoları riski büyük ölçüde kapatıyor; kalan nadir durum için `nax <metin>` kaçış yolu var |

## Kapandı

| Tarih | Ne | Nerede kapandı |
|---|---|---|
| 2026-09-30 | ~~readline etkileşimsiz girdide okuduğu satırı yankılıyor, çıktıyı ikiye katlıyordu~~ | `src/line.c` — etkileşimsiz yol `getline` kullanır, readline yalnızca terminalde çalışır |
