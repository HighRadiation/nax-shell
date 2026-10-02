# Kabuk ile naxd arasındaki protokol

## Neden bu biçim

Kayıtlar satır tabanlı, sekme ile ayrılmış `anahtar=değer` çiftleridir.
Serbest metin ve sekme/yeni satır içerebilen değerler `b64:` önekiyle
kodlanır.

Karar gerekçesi: C tarafının JSON **ayrıştırmak** zorunda kalmaması. İki ucu
da biz yazdığımız için şema sabittir; C'nin ihtiyacı sekmeden bölmek ve
base64 çözmek, yaklaşık 120 satır. Bir JSON kütüphanesi vendor etmek buna
karşılık bir derleme birimi, bir ağaç yapısı ve ömür yönetimi sorusu getirir.

`b64:` önekinin her alana uygulanmamasının sebebi ise hata ayıklama:
`id=7 kind=request status=ok` satırı günlükte okunabilir kalır. Kendi icat
ettiğin bir protokolü ayıklarken bu, kazanılan baytlardan değerlidir.

## Çerçeve

```
<TİP>\tid=<n>\tanahtar=değer\tanahtar=b64:...\n
```

Bir satır bir kayıttır. En fazla 1 MiB; aşılırsa protokol ihlali sayılır ve
`naxd` yeniden başlatılır. Bozulma durumunda bir sonraki yeni satıra kadar
atlanır — satır tabanlı olmanın asıl kazancı bu kurtarma yeteneğidir.

## Kabuktan naxd'ye

| Tip | Ne zaman | Alanlar |
|---|---|---|
| `HELLO` | Açılışta bir kez | oturum anlık görüntüsü: işletim sistemi, terminal, çalışma dizini, git dalı, en sık kullanılan başlar |
| `INTENT` | Sınıflandırıcı niyet dediğinde | `text`, `cwd`, `branch`, `status`, `since` |
| `EXPLAIN` | Başarısız komut açıklatılırken | `cmd`, `code`, varsa kabuğun kendi hata metni |
| `EVENT` | Komut çalıştıktan sonra | `seq`, `cmd`, `code`, `ms` — gönderilir ve beklenmez |
| `CANCEL` | Kullanıcı beklemeyi kesince | `id` |
| `BYE` | Çıkışta | — |

## naxd'den kabuğa

| Tip | Alanlar | Kabuk ne yapar |
|---|---|---|
| `READY` | `version`, `key`, `timeout`, `spinner` | Hazır olduğunu kaydeder; süreleri uygular |
| `OK` (`kind=request`) | `cmd`, `note`, `danger` | Öneriyi düzenleme satırına yazar |
| `OK` (`kind=question`) | `text` | Metni ekrana basar |
| `NEED` | `provide=cwd_ls,git_status` | Sabit listeden olguyu verir, en çok iki tur |
| `ERR` | `code`, `message` | Tek satır bilgi basar, kabuk çalışmaya devam eder |

Örnek alışveriş:

```
→  INTENT   id=7  text=b64:ZMO8bi...  cwd=b64:L2hvbWU...  branch=main  status=0
←  OK       id=7  kind=request  cmd=b64:ZmluZCAu...  note=b64:c29u...  danger=0
```

### `READY` neden süreleri taşıyor

`timeout` ve `spinner` yapılandırmada durur, ama yapılandırmayı **yalnızca
`naxd` okur.** Beklemeyi ise kabuk yapar. Değerleri el sıkışmada göndermek bu
çelişkiyi çözer: C tarafında ayrıştırıcı yazmak gerekmez ve tek kaynak
korunur.

`key=no` ise anahtar ayarlanmamış demektir. Kabuk bunu bilmek zorunda: istek
göndermek boş bir tur olur ve kullanıcıya oturumda **bir kez** sebebi
söylenmelidir. Alan hiç yoksa anahtar var sayılır — eski bir yardımcı süreci
hiç denememek, denemekten daha kötüdür.

Kabuk bildirilen süreleri **makul aralığa kırpar.** Karşı taraf sıfır ya da
saçma bir süre bildirirse kabuk ya hiç beklemez ya sonsuza kadar bekler;
ikisi de kabul edilemez.

## Dayanıklılık

Bu bölüm protokolün asıl içeriğidir; mutlu yol kolay olan kısım.

**Tek bekleyen istek.** Aynı anda yalnızca bir kullanıcı isteği havada olur.
Kabuk zaten cevabı bekliyor olduğu için bu kural C tarafındaki tüm eşzamanlılık
ihtiyacını ortadan kaldırır. `id` artan bir sayıdır; eski ya da tanınmayan
`id` taşıyan yanıtlar sessizce atılır.

**Zaman aşımı.** Dört saniyede bir çalışma göstergesi belirir, on beş
saniyede istek sert biçimde kesilir. Süre dolduğunda `CANCEL` gönderilir ve
satır kullanıcıya geri verilir.

**Kullanıcı beklemeyi keserse.** Bekleme, `naxd`'nin çıkışı ile bir kendine
boru üzerinde aynı anda gözlenir. Kesme işleyicisi yalnızca o boruya bir bayt
yazar — sinyal işleyicisi içinde güvenli tek iş bu.

**naxd ölürse.** Kırık boruya yazma sinyal yerine hata döndürecek biçimde
ayarlanır, böylece kabuk ölmez. Yeniden başlatma 0, 1 ve 4 saniye aralıklarla
denenir; bir dakika içinde üç başarısızlık olursa AI kapatılır, oturumda **bir
kez** tek satır uyarı basılır ve kabuk düz kabuk olarak tam işlevle devam eder.

**Yarım satır.** Okuma tamponu dört durumu ayırır: satır hazır, daha veri
gerekiyor, akış bitti, satır aşırı uzun. Akış bittiğinde elde kalan yarım
satır atılır.

**naxd'nin hata çıkışı asla terminale gitmez.** Başlatılırken bir günlük
dosyasına yönlendirilir. Aksi halde Python tarafındaki bir hata izi
kullanıcının ekranını bozardı; bu şekilde bozmaz ama kaybolmaz da.
