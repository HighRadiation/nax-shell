# v0.2.0 — planlanan özellikler

Bu klasör **v0.2.0 için planlanan beş özelliğin** her birini ayrı dosyada
tutar. Yön ve sıra [../VISION.md](../VISION.md)'de; burası o yönün
parçalarının ayrıntısı.

| Dosya | Özellik |
|---|---|
| [VOICE.md](VOICE.md) | Ses girişi, metin çıkışı |
| [MEMORY.md](MEMORY.md) | Oturumlar arası kalıcı bellek |
| [VISUAL.md](VISUAL.md) | Görsel katman |
| [OS.md](OS.md) | İşletim sistemine dönüşme |
| [PLATFORMS.md](PLATFORMS.md) | macOS ve Windows |

## Bu klasörü okurken

**Buradaki hiçbir şey ölçülmedi.** Yol haritasındaki kapanmış taşların aksine
bunlar plan. Bir özellik ölçülüp koda girdiğinde yeri burası değil,
[../ROADMAP.md](../ROADMAP.md) ve [../ARCHITECTURE.md](../ARCHITECTURE.md)
olur; bu dosya da o zaman küçülür.

**Sürüm numarası klasör adıdır, kodun sürümü değil.** `src/nax.h` içindeki
`NAX_VERSION` bugün `0.1.0` ve öyle kalıyor; bu klasör o numaranın *sonrası*
için yazılmış.

**Sıra bozulmaz.** Kabuk çekirdeği → ses → bellek → işletim sistemi. Her adım
öncekinin üstünde duruyor, yanında değil. Beş dosyanın ikisi (OS, PLATFORMS)
bu sıranın dışında ve uzun vadeli; üçü (VOICE, MEMORY, VISUAL) sıradaki iş.

## Her dosyada aranacak şey

Özellik anlatımı değil, **neyin kırılabileceği**. Bu depoda iki yasa var ve
ikisi de yeni bir katman eklenirken sessizce iptal olabilir:

1. Hiçbir komut kullanıcı görmeden çalışmaz.
2. AI kırılırsa kabuk çalışır.

Ses birinci yasayı, görsel katman ikinci yasayı tehdit ediyor. İkisi de kodda
hiçbir şey kırılmadan olur — yani test yakalamaz. Dosyalar bu yüzden "nasıl
yapılır"dan önce "ne bozulur" sorusunu cevaplıyor.
