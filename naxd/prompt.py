"""
prompt.py - modele ne soruldugu ve cevabin nasil okundugu.

SORU ILE ISTEK AYRIMI:
    Siniflandirici "bu satir dogal dil" dedigi her satiri NIYET olarak
    gonderiyor. Ama dogal dilin iki ayri turu var:

      "eski loglari sil"        -> bir KOMUT istiyor
      "bu hata ne demek"        -> bir CEVAP istiyor

    Ikisi ayni yoldan gecemez: birincisinin cevabi duzenleme satirina
    yazilir, ikincisinin cevabi ekrana basilir. Ayrimi yapan taraf
    modeldir, cunku yalnizca cumleyi anlayan karar verebilir.

CEVAP BICIMI IKI SATIR BASLIGINDAN BIRI:
      CMD <tek satir kabuk komutu>
      ANS <kisa cevap>

    NEDEN SERBEST METIN DEGIL: cevabi ayristirmak belirlenimci olmak
    zorunda. "Iste komutunuz: ls -la" gibi bir cevaptan komutu cikarmaya
    calismak tahmin isi olur ve tahmin eden bir kabuk guvenilmez. Iki
    basliktan biri ise tek bir karsilastirma.

    Model basligi atlarsa cevap CEVAP sayilir, komut sayilmaz. Sira
    bilincli: yanlis okunan bir metni ekrana basmak zararsiz, yanlis
    okunan bir komutu duzenleme satirina yazmak degil.

OLAN KOMUT AYNEN GERI DONER:
    Siniflandirici bazen gercek bir komutu yanlislikla niyet sanabilir
    (sekil vetolari bilincli olarak temkinli). O durumda yanlis
    yonlendirmenin bedeli yalnizca gecikme olsun diye, modele "gelen
    metin zaten gecerli bir kabuk komutuysa aynen geri ver" deniyor.
"""

INTENT_SYSTEM = """Sen bir kabuk yardimcisisin. Kullanicinin Turkce ya da
Ingilizce yazdigi satiri degerlendirip TAM OLARAK iki bicimden biriyle
cevap verirsin:

CMD <komut>
ANS <cevap>

CMD bir ISTEK icin: kullanici bir is yapilmasini istiyorsa, bunu
gerceklestiren TEK SATIR kabuk komutunu ver. Aciklama ekleme, kod blogu
kullanma, birden fazla satir yazma.

ANS bir SORU icin: kullanici bilgi istiyorsa en fazla iki cumlede cevapla.

Kurallar:
- Gelen metin zaten gecerli bir kabuk komutuysa aynen "CMD <metin>" dondur.
- KULLANICIYA SORU SORMA. Kabuk TEK TURLU: cevabini alamazsin, cunku
  kullanicinin yazdigi sonraki satir yeni bir satir olarak islenir ve o
  satir senin soruna cevap sanilmaz. "Hangi dizinde?" diye sorarsan
  kullanici "yerel" yazar ve kabuk onu bir komut sanip yazim duzeltmesine
  dusurur.
- Emin olamadigin bir is icin komut UYDURMA. Once en makul varsayimi yap ve
  komutu ver; varsayim yapilamiyorsa ANS ile neyin eksik oldugunu BILDIR -
  soru cumlesi kurmadan, duz cumleyle.
- Geri donusu olmayan bir is isteniyorsa komutu ver ama en guvenli bicimini
  sec (ornegin silmek yerine listelemeyi onermek gerekiyorsa ANS kullan).
- Cevabin ilk uc harfi CMD ya da ANS olmak zorunda."""

EXPLAIN_SYSTEM = """Sen bir kabuk yardimcisisin. Kullanicinin calistirdigi
komut basarisiz oldu. Neden basarisiz oldugunu en fazla uc cumlede, sade
bir dille acikla ve mumkunse ne yapmasi gerektigini soyle. Kod blogu
kullanma. Turkce cevapla."""


def intent_messages(text, context=None):
    """Niyet istegi icin mesaj listesini kurar."""
    user = text
    if context:
        user = "%s\n\n[baglam]\n%s" % (text, context)
    return [
        {"role": "system", "content": INTENT_SYSTEM},
        {"role": "user", "content": user},
    ]


def explain_messages(command, code, message=None):
    """Hata aciklamasi istegi icin mesaj listesini kurar."""
    lines = ["komut: %s" % command, "cikis kodu: %s" % code]
    if message:
        lines.append("kabugun mesaji: %s" % message)
    return [
        {"role": "system", "content": EXPLAIN_SYSTEM},
        {"role": "user", "content": "\n".join(lines)},
    ]


def read_reply(text):
    """
    Modelin cevabini (tur, icerik) ciftine cevirir.

    Tur "request" ya da "question". Baslik yoksa ya da taninmiyorsa cevap
    SORU sayilir: yanlis okunan bir metni basmak zararsiz, yanlis okunan
    bir komutu duzenleme satirina yazmak degil.
    """
    stripped = (text or "").strip()
    if not stripped:
        return ("question", "")
    head = stripped[:3].upper()
    body = stripped[3:].strip()
    if head == "CMD" and body:
        return ("request", first_line(body))
    if head == "ANS" and body:
        return ("question", body)
    return ("question", stripped)


def first_line(text):
    """
    Ilk satiri verir; kod blogu isaretlerini atar.

    Model kurala ragmen bazen uc ters tirnak ekliyor. Komut tek satir
    olmak zorunda oldugu icin fazlasini atmak, bozuk bir komutu
    kullaniciya vermekten iyidir.
    """
    for line in text.splitlines():
        clean = line.strip()
        if clean.startswith("```"):
            clean = clean.lstrip("`").strip()
            if clean.lower() in ("sh", "bash", "shell", "zsh", ""):
                continue
        if clean:
            return clean
    return ""
