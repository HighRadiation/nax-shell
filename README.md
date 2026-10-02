# NAX

AI'ı terminalin *yanına* değil, kabuğun *dilinin içine* koyan bir komut kabuğu.

Ayrı bir mod yok, özel bir işaret yok, çağrılacak bir program adı yok. Aynı
satıra hem komut hem niyet yazılır; hangisi olduğuna kabuk karar verir.

```
nax ~/projeler $ ls -la
...                                  ← normal komut: sıfır gecikme, AI'a hiç uğramaz

nax ~/projeler $ dün değişen dosyaları zip'le
  ↳ find . -newermt '1 day ago' -type f -print0 | xargs -0 zip degisen.zip
nax ~/projeler $ find . -newermt '1 day ago' ...
                                     ← öneri düzenleme satırına yazılır, Enter çalıştırır

nax ~/projeler $ bu dizin ne işe yarıyor
Kabuğun kaynak kodu burada; src/ altında okuma döngüsü ve satır yönetimi var.
                                     ← soru: metin cevap, çalıştırılacak bir şey yok
```

## Neden bu, "terminalde AI çalıştırmak"tan farklı

Ayrı bir AI programına "şu hatayı çöz" dediğinde o program dosya sormak, `ls`
çekmek, tahmin etmek zorundadır. Kabuğun içindeki AI ise çalışma dizinini, son
çalıştırdığın komutları, çıkış kodlarını ve git dalını **hiç sormadan** bilir.

Fark kodun nerede durduğu değil, paylaşılan durum.

## İki kural

**Hiçbir komut sen görmeden çalışmaz.** AI'ın ürettiği satır düzenleme
tamponuna yazılır; çalıştıran şey senin Enter'a basmandır. Yıkıcı komutlarda
ayrıca açık onay istenir.

**AI çökerse kabuk çalışır.** Ağ giderse, anahtar yoksa, yardımcı süreç
ölürse kabuk tek satır uyarı basar ve düz bir kabuk olarak tam işlevle devam
eder. Bu bir kabuk; AI onun bir aşaması.

## Kurulum

Gereken tek harici bağımlılık readline geliştirme başlıkları:

```
sudo apt-get install -y libreadline-dev
make
./nax
```

AI tarafı için yapılandırma:

```
cp nax.conf.example nax.conf
chmod 600 nax.conf          # api_key satirini doldur
```

`nax.conf` olmadan da kabuk tam çalışır; yalnızca AI yolu kapalı kalır.

## Geliştirme

```
make            kabugu derler, uyarilar hata sayilir
make asan       adres ve tanimsiz davranis denetleyicili ikili
make test       testleri normal ikili ile kosar
make check      testleri her iki ikili ile kosar
make clean      uretilen her seyi siler
```

Bu ortamda `valgrind` yok; bellek doğrulaması `make check` üzerinden
denetleyicilerle yapılır. Sızıntı sıfır olmadan hiçbir aşama bitmiş sayılmaz.

## Durum

Çekirdek kabuk çalışıyor: okuma döngüsü, renkli prompt, kalıcı geçmiş,
`exit`, Ctrl-D ve sinyaller — Ctrl-C yarım satırı atıp temiz bir prompt
verir, Ctrl-\ yok sayılır. **Komutlar artık gerçekten koşuyor** — tek komut,
`PATH` çözümleme, gerçek çıkış kodları ve bash ile birebir eşleşen hata
mesajları:

```
nax ~/projeler $ echo merhaba dunya
merhaba dunya
nax ~/projeler $ printf "[%s]" $BOSLUKLU
[bir][iki]
nax ~/projeler $ boylebirkomutyok
nax: boylebirkomutyok: command not found
nax ~/projeler $ echo $?
127
```

**Borular ve yönlendirmeler de çalışıyor:**

```
nax ~/projeler $ printf "c\na\nb\n" | sort | head -2 | tr "\n" ","
a,b,
nax ~/projeler $ wc -l < girdi.txt > sayim.txt
nax ~/projeler $ true | false
nax ~/projeler $ echo $?
1
```

**Yerleşikler de çalışıyor** — `cd`, `echo`, `pwd`, `export`, `unset`, `exit`:

```
nax ~ $ cd projeler
nax ~/projeler $ export AD=dunya
nax ~/projeler $ echo -n "merhaba $AD"
merhaba dunya
```

`cd` ve `export` ana süreçte koşar, yoksa değişiklik çocukla birlikte yok
olurdu.

**Kabuk artık her satırı kendisi sınıflandırıyor** — hiçbir işaret, hiçbir
kip yok. Normal komutlar bu yoldan hiç sapmadan geçiyor; doğal dil ayrı yola
gidiyor:

```
nax ~/projeler $ ls -la                      # komut, doğrudan koşar
nax ~/projeler $ find all big files          # doğal dil, niyet yoluna gider
nax ~/projeler $ cat sirket.txt              # "cat" + var olan dosya: komut
```

Önemli olan şu: `ls -la` yazıldığında ne gecikme ne ücret oluşuyor. AI yalnızca
kabuğun anlamlandıramadığı satırlarda devreye giriyor. Kararın nasıl
verildiği [docs/CLASSIFIER.md](docs/CLASSIFIER.md) içinde.

**Yazım hataları AI'a hiç gitmeden yerelde düzeltiliyor:**

```
nax ~/projeler $ celar
nax: celar: boyle bir komut yok
nax: bunu mu demek istediniz: clear
nax ~/projeler $ clear▮        ← düzeltilmiş satır tampona hazır gelir
```

Geri dönüşü olmayan komutlar (`rm`, `dd`, `chmod`, `kill` ve benzeri) bu
istisnanın dışında: önerilirler ama **tampona konulmazlar.** Tampona konan
öneri tek Enter'la koşar ve bu tür bir komut için o fazla yakın.

**Yardımcı süreç gerçek, ama dayanıklılığı taklitle ölçüldü.** Karşı tarafta
kasten kötü davranan bir süreç koşturuluyor, çünkü gerçek bir model ölmeyi,
donmayı ve saçmalamayı sipariş üzerine yapmaz. Sorulan soru şu: yardımcı
süreç ölürse, donarsa ya da saçmalarsa kabuk sağlam kalıyor mu? Ölçülen cevap:

| Durum | Kabuk ne yapıyor |
|---|---|
| Süreç ölür | Bağlantıyı kapatır, yaşamaya devam eder |
| Hiç cevap vermez | Zaman aşımı, satır kullanıcıya geri döner |
| Okumayı bırakır | Yazmayı da sınırlı bekler, kilitlenmez |
| Bozuk konuşur | İhlal sayar, süreci yeniden başlatır |

`Ctrl-C` beklemeyi keser: kesme işleyicisi bir boruya tek bayt yazar, çünkü
sinyal bağlamında güvenle yapılabilecek tek iş bu.

**Doğal dil artık gerçekten çalışıyor.** Yardımcı süreç yalnızca Python
standart kütüphanesiyle yazıldı; paket kurmak gerekmiyor.

```
nax ~/projeler $ dun degisen dosyalari goster
nax: find . -newermt "1 day ago" -type f
nax ~/projeler $ find . -newermt "1 day ago" -type f▮   ← tampona hazır gelir

nax ~/projeler $ boylebirkomutyok
nax: boylebirkomutyok: command not found
nax ~/projeler $ ?
Komut bulunamadi, cunku PATH icinde boyle bir program yok.
```

Doğal dilin iki türü ayrı yollardan geçer: **istek** bir komut önerir ve
tampona hazırlanır, **soru** ekrana basılır. Ayrımı model yapar ama kabuk
ona körü körüne güvenmez:

- Riskli komut önerilir, **tampona konulmaz** — karşı taraf "güvenli" dese
  bile, çünkü kabuk komutun başını kendi listesiyle de denetler.
- `api_key` boşsa sebep bir kez yazılır ve kabuk **düz kabuk** olarak çalışır:
  satır bash'in yaptığı şeye düşer. Anahtarsız durum bir hata hâli değil.
- Yardımcı süreç ölürse, donarsa ya da saçmalarsa kabuk sağlam kalır.

Kurulum: `cp nax.conf.example nax.conf`, `chmod 600 nax.conf`, `api_key`
satırını doldur. Ayarı kaydettiğin an geçerli olur; kabuğu yeniden başlatmak
gerekmez. Ayrıntısı [docs/CONFIG.md](docs/CONFIG.md)'de.

**İçerideki AI artık sormadan biliyor:** çalışma dizini, son komutlar ve
çıkış kodları, git dalı, en sık kullandığın araçlar. Terminalde ayrı bir
program olarak koşan bir yardımcı bunları bilemez.

Ne gönderildiğini görmek için tahmin etmen gerekmiyor:

```
nax ~/nax-shell $ ctx
--- her istekte gidiyor ---
dizin: ~/nax-shell
git dali: main
son komutlar:
  [0] echo bir
  [127] boylebirkomutyok
--- yalniz oturum acilisinda gitti ---
ortam degiskeni adlari: PATH, HOME, TERM, ...
--- bunun disinda hicbir sey ---
```

Gizlilik sözleşmesi dört kuralla uygulanıyor — ayrıntısı
[docs/PRIVACY.md](docs/PRIVACY.md)'de:

- **Sır temizleme kabuk tarafında**, veri yardımcı sürece verilmeden önce.
  Hatalı ya da ele geçirilmiş bir süreç, hiç almadığı veriyi sızdıramaz.
- **Boşlukla başlayan satır bağlama hiç girmez** — bağlamdan kaçmanın yolu.
- Ortam değişkenlerinin **yalnızca adları** gider, değerleri asla.
- Belirli dizinlerde AI **tamamen kapatılabilir**; kararı kabuk verir, yani
  veri karşı tarafa hiç ulaşmaz.

**Kabuk dili tamamlandı:** `;`, `&&`, `||`, `<<` ve dosya adı genişletmesi.
Davranışın tamamı bash ile karşılaştırılarak ölçüldü.

```
nax ~/projeler $ make && ./test/run.sh || echo "patladi"
nax ~/projeler $ echo *.c
a.c b.c
nax ~/projeler $ cat << SON
> iki satir
> SON
iki satir
```

**Çevrimdışı da işe yarıyor.** Sık kullanılan niyetlerin elle yazılmış
karşılığı var ve modelden önce ona bakılıyor — sıfır gecikme, sıfır ücret:

```
nax ~/projeler $ disk kullanimini goster
nax: df -h
```

Çevrimdışı değerin büyük kısmı dil modelinden değil, bu belirlenimci katmandan
geliyor: yazım düzeltmesi, PATH çözümlemesi, sınıflandırıcı ve niyet tablosu.
Yerel dil modeli bunun **üstünde** opsiyonel bir katman, altında değil.

Claude için `provider = anthropic` yeterli; farkları kod tarafında ele alındı.

Planın tamamı bitti — ama bu projenin bittiği anlamına gelmiyor. Neyin
ölçülmediği [docs/ROADMAP.md](docs/ROADMAP.md)'de açıkça yazılı.

Bilinen eksikler [docs/FINDINGS.md](docs/FINDINGS.md) içinde kayıtlı.

## Ne yapmaz

Bunlar eksik değil, sınır. Bilerek böyle:

- **Sohbet botu değildir.** Her satır kendi başına durur: bir istek bir komut
  üretir, bir soru iki cümlelik cevap alır. Modelin verdiği cevap bağlama
  girmez, yalnızca **çalıştırdığın komutlar** girer. Yani "peki onu nasıl
  silerim" diye devam edemezsin; soruyu kendi başına ayakta duracak şekilde
  yazman gerekir. Bunun sebebi kabuğun sözleşmesi: bir satır, bir karar.
- **Hiçbir şeyi kendi başına çalıştırmaz.** Öneri düzenleme satırına yazılır;
  çalıştıran şey senin Enter'a basmandır. Yıkıcı komutlar tampona hiç konmaz.
- **Arka plan işi (`&`) ve alt kabuk (`( )`) yok.** Sözcük ayırıcı tanıyor,
  ayrıştırıcı anlaşılır bir hata veriyor.
- **Önerilen komutların kalitesi ölçülmedi.** Protokol, dayanıklılık,
  gizlilik, sınıflandırıcı ve kabuk dili vaka tablolarıyla ölçülü; modelin
  ürettiği komutun işe yarayıp yaramadığı ölçülü **değil**, çünkü bütün AI
  testleri sahte sağlayıcıya karşı koşuyor (ağ yok, anahtar yok, ücret yok).
  Bu bilinçli bir boşluk ve [docs/ROADMAP.md](docs/ROADMAP.md) içinde yazılı.

## Bir şey ters giderse

AI yolu sustuğunda ya da `nax: AI: servis 4xx dondurdu` yazdığında sebep tek
satırda görünür; tam yanıt `~/.nax-naxd.log` dosyasındadır. En sık iki sebep
ve çözümü [docs/CONFIG.md](docs/CONFIG.md) içindeki "Sorun giderme"
bölümünde. Kabuk bu durumların hiçbirinde durmaz: düz kabuk olarak çalışır.

## Dokümanlar

| Dosya | İçerik |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Boru hattı, süreç modeli, bellek sahipliği |
| [docs/CLASSIFIER.md](docs/CLASSIFIER.md) | "Komut mu niyet mi" kararı, vetolar, tuzaklar |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | Kabuk ile yardımcı süreç arasındaki protokol |
| [docs/CONFIG.md](docs/CONFIG.md) | Sağlayıcılar, yerel model, düşme kuralı |
| [docs/PRIVACY.md](docs/PRIVACY.md) | Neyin gönderildiği ve neyin gönderilmediği |
| [docs/NORMS.md](docs/NORMS.md) | Kod stili sözleşmesi |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Nerede kaldık, sırada ne var |
| [docs/FINDINGS.md](docs/FINDINGS.md) | Ertelenmiş işler |

## Lisans

**GPL-3.0-or-later** — tam metin [LICENSE](LICENSE) dosyasında.

Seçim serbest değildi: `nax` GNU readline'a bağlanıyor ve readline GPLv3
veya sonrası. Dağıtılan ikili, bu kod ile readline'ın birleşik eseri; o eserin
GPLv3 şartlarıyla dağıtılabilir olması gerekiyor. Kaynağı izin verici bir
lisansa koymak mümkündü ama o zaman kaynak bir lisansta, derlenmiş program
başka bir lisansta olurdu — okuyanın kaçırmaması gereken bir nüans. Aynı
lisansta olmak belirsizliği bitiriyor. Üstelik bash da GPLv3, ve bu proje
davranışını baştan sona bash ile karşılaştırarak ölçüyor.

Bir sebep daha var: bu kabuğun gizlilik sözleşmesi ("sır temizleme kabuk
tarafında", "ortam değişkenlerinin yalnızca adları gider") ancak kaynak açık
kalmak **zorundaysa** denetlenebilir. Birisi nax'ı kapalı bir ürüne gömebilse,
o sözleşmenin doğrulanabilirliği kaybolurdu.

Pratikte: kullan, değiştir, dağıt. Değiştirdiğin bir sürümü dağıtıyorsan
kaynağını da aynı lisansla vermen gerekiyor.

Telif hakkı © 2026 Buğra Öksüz.
