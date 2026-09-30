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

Baş çözülmediğinde ilk iş AI'a gitmek değildir. Gerçek kullanımdaki yazım
hatalarının neredeyse tamamı, sık kullanılan bir komuta iki karakterden az
uzaklıktadır (`celar` → `clear`, `exot` → `exit`, `gti` → `git`). Bunları
uzak bir modele göndermek hem israf hem yüz milisaniyelerce gecikme.

Bu yüzden aşama 4 yerel bir düzenleme uzaklığı hesabıyla çalışır: aday kümesi
yerleşik komutlar + PATH + geçmişte kullanılan başlar; geçmiş sıklığı
eşitlikleri bozar.

İki katı kural:

1. **Satırın tamamı kabuk şeklinde olmalı** (sözcük sayısı ≤ 3, argümanlar
   seçenek ya da yol görünümünde). Aksi halde düzeltme atlanır. Gerekçesi
   aşağıdaki tuzak tablosunda.
2. **Yıkıcı başlar asla önerilmez.** `rm`, `dd`, `mkfs*`, `shred`, `chown`,
   `chmod`, `kill`, `mv`, `truncate` bir düzeltme sonucu olarak düzenleme
   satırına yazılmaz. Gerekçe basit: tamponda duran `rm -rf` bir tuş
   uzaklıktadır.

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
