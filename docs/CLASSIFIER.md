# Sınıflandırıcı

Projenin kalbi burasıdır. Kullanıcı özel bir işaret yazmadığı için, her satır
için "bu bir komut mu, yoksa bir niyet mi" kararını kabuk kendi vermek
zorundadır.

## Karar sırası

Ucuzdan pahalıya; ilk eşleşen kazanır ve sonraki aşamalar hiç çalışmaz.

| # | Aşama | Sonuç | Maliyet | AI çağrısı |
|---|---|---|---|---|
| 0 | Ön filtre: boş satır, denetim karakteri artığı, `\` öneki | yok say / komut | <1 µs | yok |
| 1 | Sözcüklere ayırma: özel karakter var mı, tırnak kapanmış mı | — | ~10 µs | yok |
| 2 | Baş çözümü: yerleşik komut → `/` içeriyorsa dosya → PATH önbelleği | **komut** | ~5 µs | yok |
| 3 | Şekil vetosu: baş çözüldü ama satır doğal dil mi | niyete devret | ~10 µs | yok |
| 4 | Yerel düzeltme: yazım hatası tamiri | **öneri** | <2 ms | yok |
| 5 | Kalan her şey | **niyet** | ~1 sn | 1 |

Tasarımın en önemli sonucu: **normal komutlar AI'a hiç uğramaz.** `ls -la`
yazdığında ne gecikme ne ücret oluşur. AI yalnızca kabuğun zaten
anlamlandıramadığı satırlarda devreye girer.

## Aşama 3 — şekil vetosu

"İlk kelime PATH'te çözülüyorsa bu bir komuttur" kuralı sezgisel olarak doğru
görünür ama **tek başına yetersizdir.**

Asıl tehlike Türkçe kelimeler değil, PATH'te gerçekten var olan İngilizce emir
kipleridir: `open`, `find`, `make`, `test`, `time`, `kill`, `free`, `who`,
`last`, `sort`, `watch`, `install`, `top`, `head`, `tail`, `tar`, `tee`, `yes`,
`w`, `id`, `ip`. Yani şu satırların hepsi, o kural tek başına uygulanırsa,
komut sanılır:

```
find all big files
make it faster
who is using port 3000
free up space
```

Bu yüzden baş çözülse bile aşağıdaki vetolardan biri geçerse satır niyet
yoluna gider.

**Vetolardan önce gelen tek kural:** satırda kabuk operatörü (`|`, `>`, `<`,
`>>`, `&&`) varsa hiçbir vetoya bakılmaz, satır kabuk sayılır. Boru ve
yönlendirme doğal dilde geçmez, bu yüzden baş hiç çözülmese bile güçlü bir
komut işaretidir. Bu kural sonradan eklendi: olmadan `a && b` ve `> dosya`
satırları niyet sanılıyor, kabuğun sözdizimi hatası hiç görünmüyordu.

| Veto | Kural |
|---|---|
| V1 | Satır `?` ile bitiyor. **Şart:** `?` işaretinden önce boşluk var ya da satır en az üç sözcük. Aksi halde `ls foo?` gibi joker karakterli bir komut soru sanılırdı |
| V2 | Bir sözcük ASCII dışı bayt içeriyor (ç ğ ı ö ş ü). **Baş dahil sayılır:** Türkçe karakterli bir baş hiçbir komuta karşılık gelmiyor. **İstisna:** o sözcük var olan bir şeyi adlandırıyorsa veto uygulanmaz — `cat şirket.txt` çalışır |
| V3 | Çıplak kelime torbası: sözcük sayısı ≥ 4 **ve** hiçbiri `-` ile başlamıyor **ve** hiçbiri `/ . * = $ ~ [ ] % { } , :` içermiyor **ve** hiçbiri var olan bir yol değil |
| V4 | Durak kelime listesinden en az bir eşleşme ve sözcük sayısı ≥ 2. **Baş sayılmaz** ve var olan yollar sayılmaz |
| V5 | Baş, kendi doğrulayıcısı olan bir baş ve doğrulayıcı başarısız: `git` → argüman alt komut listesinde değil; `find` ve `test` → argüman var olan bir yolu adlandırmıyor; `make` → dizinde `Makefile` yok |

V3'ün karakter kümesi ölçülerek genişletildi. İlk yazımda yalnızca `/.*=$~`
vardı ve `printf "[%s]" a b c` satırı dört çıplak kelime sanılıp niyet yoluna
gidiyordu. Köşeli parantez, yüzde, süslü parantez, virgül ve iki nokta doğal
dilde geçmez; biçim dizelerinde ve argümanlarda geçer.

### V4'ün iki sınırı

**Baş sayılmaz.** `who` ve `which` hem gerçek komut hem soru kelimesidir. Baş
olarak geçtiğinde komuttur — bu sınır olmasa `who -b` satırı niyet sanılırdı.
Buna karşılık `who is using port 3000` satırında `who` baş olduğu için durak
kelime sayılmaz ama satır zaten V3'e takılır.

**Tek harfli tanımlayıcılar listede yok.** `a` ve `an` bilinçli olarak
dışarıda: tek harfli kelimeler dosya adı ve argüman olarak sık geçer. Listede
oldukları sürece `cd a b` satırı niyet sanılıyordu.

Veto tetiklendiğinde satır **sessizce** niyet yoluna gider. `naxd`'nin sistem
isteminde şu kural bulunur: *"gelen metin zaten geçerli bir kabuk komutuysa
aynen geri ver."* Böylece yanlış yönlendirmenin bedeli birkaç yüz milisaniye
olur — **yanlış çalıştırma olmaz.** Tasarımın her yerinde tercih bu yöndedir:
yavaşlık kabul edilebilir, sessiz yanlış davranış edilemez.

## Aşama 4 — yerel yazım düzeltmesi

Baş çözülmediğinde ilk iş AI'a gitmek değildir. Yazım hatalarının neredeyse
tamamı sık kullanılan bir komuta iki karakterden az uzaklıktadır. Bunları uzak
bir modele göndermek hem israf hem yüz milisaniyelerce gecikme.

Aday kümesi **yerleşikler + PATH**; sıralama önce uzaklık, eşitlikte geçmiş
sıklığı. Yerleşikler önce taranır, yani eşitlikte yerleşik kazanır — `ehco`
için `echo` doğru cevap.

### Uzaklık ölçüsü neden Damerau-Levenshtein

Düz Levenshtein'de komşu iki harfin yer değiştirmesi **iki** adım sayılır.
Oysa en sık daktilo hatası tam olarak budur. Yer değiştirmeyi tek adım sayan
ölçü, eşiği büyütmek zorunda kalmadan bu hataları yakalıyor. Bu kapta ölçülen
sonuçlar:

| Yazılan | Öneri | Uzaklık |
|---|---|---|
| `celar` | `clear` | 1 |
| `mkae` | `make` | 1 |
| `gerp` | `grep` | 1 |
| `exprot` | `export` | 1 |
| `systemclt` | `systemctl` | 1 |
| `pyhton` | `python3` | 2 |

### Üç sınır ve hepsinin ölçülmüş sebebi

| Sınır | Değer | Sebep |
|---|---|---|
| En kısa baş | 3 harf | `a` sözcüğünün PATH'te 1 uzaklıkta onlarca karşılığı var. Sınır olmasa `a && b` satırı `w && b` önerisi alırdı |
| En uzun baş | 24 harf | DP tablosu sabit boyutlu; değişken uzunluklu dizi yasak |
| Eşik | ≤ 4 harf için 1, üstü için 2 | Kısa sözcükte 2 adım çok geniş: tek harfi ortak olan her şeyi aday yapıyor |

### İki katı kural

**1. Düzeltme yalnızca satır kabuk şeklindeyse denenir**, yani **hiçbir şekil
vetosu tetiklenmemişse.** Bu, planda "sözcük sayısı ≤ 3" diye yazılmıştı;
uygulamada veto kontrolü bunun yerini aldı çünkü daha doğru ölçüyor.

Gerekçe ölçüldü: `dun` sözcüğünün `du` komutuna uzaklığı **1**. Bu kural
olmasa `dun degisen dosyalari goster` isteği disk kullanımı komutu sanılırdı.
Satırda dört çıplak kelime olduğu için V3 tetikleniyor ve düzeltme hiç
denenmiyor.

Aynı kural `sil eski loglari` satırını da korur — ama farklı yolla: o satırda
veto tetiklenmiyor, düzeltme deneniyor ve **`sil` için aday bulunamıyor**
(ölçüldü). Satır bu yüzden sözcük sayısı kuralına düşüp niyet oluyor.

**2. Geri dönüşü olmayan komutlar önerilir ama tampona KONULMAZ.**

Bu ayrım kritik ve planda yanlış yazılmıştı ("asla önerilmez"). Öneri
*yazılır* — kullanıcı `chmdo` yazdığında `chmod` demek istediğini bilmeli.
Yazılmayan şey öneriyi **bir sonraki promptun düzenleme tamponuna** koymaktır:
tampona konan öneri tek Enter'la koşar, ve `rn -rf .` için `rm -rf .`
hazırlamak refleks bir tuşla geri alınamayan silme demek.

Liste kasten kısa, yalnız geri alınamayan işlere bakıyor: dosya silen
(`rm`, `rmdir`, `shred`), üzerine yazan (`dd`, `truncate`, `mkfs*`, `mkswap`,
`fdisk`, `parted`), izin ve sahiplik değiştiren (`chmod`, `chown`, `chgrp`),
süreç öldüren (`kill`, `killall`, `pkill`), taşıyan (`mv`), makineyi kapatan
(`reboot`, `shutdown`, `halt`, `poweroff`) ve kullanıcı silen (`userdel`,
`groupdel`).

Bu kuralın testi pty üzerinden koşuyor ve mutasyonla doğrulandı: koruma
kaldırıldığında "riskli öneri tampona KONULMADI" vakası patlıyor.

### Öneri kabul edilmediğinde

Hiçbir şey koşmadığı için çıkış kodu **127** — bash'in "command not found"
cevabı. Öneri yazılmış olması bunu değiştirmiyor.

## Bilinen tuzaklar

| Durum | Sorun | Savunma |
|---|---|---|
| `git dalımı söyle` | `git` PATH'te | V5 (alt komut değil) + V2. ASCII yazımı `git dalimi soyle` yalnızca V5'e takılır |
| `find all big files` | `find` PATH'te | Üçü birden: V3 (dört çıplak kelime) + V4 (`all`) + V5 (`all` bir yol değil) |
| `dun degisen dosyalari goster` | ASCII yazılmış Türkçe; `dun` çözülmez ve `du`'ya uzaklığı 1 | Düzeltme yalnızca satır kabuk şeklindeyse çalışır; 4 çıplak kelime bu koşulu bozar |
| `git'e dokunma`, `don't touch it` | Kapanmamış tırnak → devam satırı beklerken kilitlenme | Devam moduna yalnızca satırda özel karakter varsa ya da baş çözülüyorsa girilir |
| Terminalin stdin'e sızdırdığı fare raporu | Her sızan rapor bir AI çağrısına dönüşürdü | Aşama 0 denetim karakteri içeren satırı düşürür |
| Betik modu (terminal değil) | — | AI tamamen kapalı; çözülmeyen baş `command not found` ve çıkış kodu 127 |

### Sonradan ölçülen yanlış pozitifler

Aşağıdaki dört satır sınıflandırıcı ilk kez kabuğa bağlandığında **komut
olmasına rağmen niyet sanıldı.** Hepsini bütünleşik testler yakaladı, yani
korpus tek başına yetmedi: korpus sınıflandırıcının kendi diliyle yazılmış,
bütünleşik testler ise kabuğun gerçek davranışını ölçüyor.

| Satır | Neden yanlış sınıflandı | Düzeltme |
|---|---|---|
| `printf "[%s]" a b c` | `[%s]` çıplak kelime sanıldı → V3 | V3'ün karakter kümesine `[ ] % { } , :` eklendi |
| `cd a b` | `a` durak kelime listesindeydi → V4 | Tek harfli tanımlayıcılar listeden çıkarıldı |
| `a && b` | Baş çözülmüyor, iki sözcükten fazla | Operatör kuralı: operatör varsa kabuk |
| `> dosya` | İlk token sözcük değil, baş "çözülmedi" sayıldı | Aynı operatör kuralı |

Bir de kod bulgusu: `veto_words` içinde "argüman görünümlü sözcükler durak
kelime sayılmasın" diye bir koruma vardı. Mutasyon testi onu silmenin hiçbir
testi bozmadığını gösterdi — çünkü **ulaşılamazdı.** Durak kelimelerin
hiçbirinde `-` öneki ya da yol karakteri yok, yani `is_stop_word` doğruyken o
koruma her zaman yanlış dönüyordu. Test edilemeyen ölü kodu tutmak yerine
silindi.

## Kaçış yolları

Normal kullanımda hiçbir işaret yazılmaz. Bunlar yalnızca nadir çakışmalar
için vardır:

| Yazım | Anlamı |
|---|---|
| `nax <serbest metin>` | Niyeti zorlar; sınıflandırıcı atlanır |
| `\ls -la` | Kabuğu zorlar; hiç sınıflandırma yapılmaz |
| `?` (tek başına) | Son hatayı açıklat |

## Niyet iki yola ayrılır

"Bir şey sormak" ile "bir şeyin yapılmasını istemek" aynı şey değildir:

| Yazdığın | Tür | Yanıt | Kabuk ne yapar |
|---|---|---|---|
| `bu dizin ne işe yarıyor` | soru | metin | ekrana basar, düzenleme satırı boş kalır |
| `dün değişen dosyaları zip'le` | istek | komut | öneriyi düzenleme satırına yazar |

Ayrımı `naxd` yapar ve yanıtta belirtir; kabuk yalnızca gösterir. Böylece bu
kuralın nasıl çalıştığı tek bir yerde durur ve kabuk yeniden derlenmeden
değiştirilebilir.

## Korpus kendi kendini besler

Her sınıflandırma kararı yerel bir dosyaya yazılır (bu dosya hiçbir yere
gönderilmez): satır, karar, tetiklenen vetolar, harcanan süre. Gerçek
kullanımda çıkan yanlış sınıflandırmalar test korpusuna terfi ettirilir.
Sınıflandırıcı böylece hiçbir makine öğrenmesi olmadan, gerçek kullanımdan
iyileşir.
