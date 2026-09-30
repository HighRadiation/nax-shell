#!/usr/bin/env python3
"""
Sahte terminal (pty) uzerinden etkilesimli yol testleri.

NEDEN VAR:
    run.sh icindeki vakalarin hepsi girdiyi boru ile veriyor. Boru terminal
    olmadigi icin kabuk etkilesimsiz yolu kosuyor; readline, gecmis, prompt
    uretimi ve sinyal yonetimi o testlerde HIC calismiyor. Yani "sizinti yok"
    iddiasi kodun yalnizca yarisini kapsiyordu.

    Bu betik ikiliyi gercek bir pty icinde baslatir, tus vurusu yazar ve
    ekranda ne olustugunu dogrular. NAX_BIN denetleyicili ikiliyi
    gosterdiginde etkilesimli yol da sizinti denetiminden gecer.

NEDEN PROMPTA SENKRON:
    Sabit sleep ile yazilan pty testleri yukun altinda kararsiz olur, ama
    yalnizca "beklenen metin gorundu mu" diye bakmak da yetmiyor: metin
    gorundugu anda kabuk henuz okumaya geri donmemis olabilir ve o araliga
    yazilan tuslar kaybolur. Bu ilk surumde gercekten oldu ve kabukta
    olmayan bir hata varmis gibi gorundu.

    Bu yuzden her gonderimden once yeni bir promptun basilmasi beklenir.
    Prompt, kabugun "okumaya hazirim" demesinin tek guvenilir isaretidir.

Kullanim:
    ./test/pty_drive.py                     ./nax dener
    NAX_BIN=./nax-asan ./test/pty_drive.py  denetleyicili ikiliyi dener
"""

import os
import pty
import re
import select
import shutil
import signal
import sys
import tempfile
import time

PROMPT = "$ "
SANITIZER_MARKS = (
    "ERROR: AddressSanitizer",
    "ERROR: LeakSanitizer",
    "runtime error:",
    "SUMMARY: AddressSanitizer",
)
ANSI = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")

GREEN = "\033[0;32m"
RED = "\033[0;31m"
OFF = "\033[0m"


class Shell:
    """Bir pty icinde kabugu surer ve ekrani biriktirir."""

    def __init__(self, binary, env=None):
        self.binary = binary
        self.buf = ""
        self.seen_prompts = 0
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.environ["TERM"] = "xterm-256color"
            os.environ["ASAN_OPTIONS"] = "detect_leaks=1"
            if env:
                os.environ.update(env)
            os.execv(binary, [binary])

    def pump(self, timeout):
        """Verilen sure boyunca ekrana geleni okur."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            ready, _, _ = select.select([self.fd], [], [], 0.05)
            if not ready:
                continue
            try:
                chunk = os.read(self.fd, 8192)
            except OSError:
                return
            if not chunk:
                return
            self.buf += chunk.decode(errors="replace")

    def wait_for(self, text, timeout=5.0, skip=0):
        """Metin ekranda en az (skip+1) kez gorunene kadar bekler."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.screen().count(text) > skip:
                return True
            self.pump(0.1)
        return self.screen().count(text) > skip

    def quiet(self, span=0.06, timeout=1.5):
        """
        Ekrana bir sure yeni bayt gelmemesini bekler.

        Bu, kabugun okumaya girdiginin en iyi gozlenebilir isaretidir:
        promptu bastiktan sonra susuyorsa artik tus bekliyor demektir.
        Sabit bir sleep yerine bunu kullanmak hem daha hizli hem daha
        guvenilir.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            before = len(self.buf)
            self.pump(span)
            if len(self.buf) == before:
                return True
        return False

    def sync(self, timeout=5.0):
        """
        Yeni bir prompt basilip ekran susana kadar bekler.

        NEDEN IKI KOSUL: promptun gorunmesi, readline'in terminali
        hazirlayip okumaya girdigini GARANTI ETMEZ. O pencereye yazilan tus
        vurusu kaybolur; kaybolan tus tamponda yarim metin birakirsa
        arkasindan gelen Ctrl-D de EOF degil karakter silme islevi gorur ve
        kabuk hic kapanmaz. Olculdu.
        """
        target = self.seen_prompts + 1
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.screen().count(PROMPT) >= target:
                self.seen_prompts = target
                self.quiet()
                return True
            self.pump(0.05)
        return False

    def send(self, data):
        os.write(self.fd, data)

    def ask(self, data, timeout=5.0):
        """Prompt bekler, sonra yazar. Kaybolan tus vurusunu onleyen tek yol."""
        ok = self.sync(timeout)
        os.write(self.fd, data)
        return ok

    def screen(self):
        """Ekrani ANSI kaclari ve satir basi karakterleri atilmis halde verir."""
        return ANSI.sub("", self.buf).replace("\r", "")

    def reap(self):
        """Surec bittiyse nasil bittigini dondurur, bitmediyse None."""
        done, status = os.waitpid(self.pid, os.WNOHANG)
        if done == 0:
            return None
        if os.WIFSIGNALED(status):
            return False, "sinyal %d ile oldu" % os.WTERMSIG(status)
        return False, "cikti, kod %d" % os.WEXITSTATUS(status)

    def close(self, timeout=2.0):
        """
        Surecin nasil bittigini dondurur: (yasiyor_mu, aciklama).

        Cikmadiysa once yeni satir, sonra dosya sonu gonderir. Yeni satir
        ONCE gitmek zorunda: tamponda yarim bir metin kalmissa Ctrl-D EOF
        yerine karakter silme islevi gorur ve kabuk hicbir zaman kapanmaz.

        OSError'DA HEMEN OLDURMEK YANLIS: pty'ye yazmanin patlamasi cogu
        zaman cocugun COKTAN ciktigi anlamina gelir. Bunu "hala yasiyor"
        diye raporlamak, kabukta olmayan bir hatayi varmis gibi gosterir;
        bu tuzak olculdu ve testin kendisini kararsiz yapiyordu. Dogrusu
        yazma patladiginda da surecin durumunu bir sure daha sormaktir.
        """
        for _ in range(4):
            self.pump(timeout / 4)
            status = self.reap()
            if status is not None:
                return status
            try:
                os.write(self.fd, b"\n\x04")
            except OSError:
                break
        for _ in range(10):
            status = self.reap()
            if status is not None:
                return status
            time.sleep(0.05)
        os.kill(self.pid, signal.SIGKILL)
        os.waitpid(self.pid, 0)
        return True, "hala yasiyor (asili kaldi)"


class Report:
    """Vaka sonuclarini toplar ve ozet basar."""

    def __init__(self):
        self.passed = 0
        self.failed = 0

    def ok(self, name):
        self.passed += 1
        print("  %sgecti%s   %s" % (GREEN, OFF, name))

    def bad(self, name, detail):
        self.failed += 1
        print("  %spatladi%s %s" % (RED, OFF, name))
        for line in str(detail).split("\n"):
            print("    " + line)

    def check(self, name, cond, detail=""):
        if cond:
            self.ok(name)
        else:
            self.bad(name, detail)


def sanitizer_report(screen):
    """Ekranda denetleyici raporu varsa ilgili parcayi dondurur."""
    for mark in SANITIZER_MARKS:
        at = screen.find(mark)
        if at >= 0:
            return screen[at:at + 1200]
    return ""


def case_echo_and_exit(binary, rep):
    """Prompt basiliyor, komut gercekten kosuyor, exit sessizce cikiyor."""
    sh = Shell(binary)
    sh.ask(b"echo merhaba\n")
    got = sh.wait_for("merhaba", skip=1)
    sh.ask(b"exit\n")
    alive, info = sh.close()
    rep.check("prompt basildi ve komut kostu", got, sh.screen()[-200:])
    rep.check("terminalde duzgun kapandi", not alive, info)
    return sh.screen()


def case_history(binary, rep):
    """Yukari ok onceki satiri geri getiriyor."""
    sh = Shell(binary)
    sh.ask(b"echo tarihce-vakasi\n")
    sh.wait_for("tarihce-vakasi", skip=1)
    sh.ask(b"\x1b[A")
    recalled = sh.wait_for("tarihce-vakasi", skip=2)
    sh.send(b"\n")
    sh.wait_for("tarihce-vakasi", skip=3)
    sh.ask(b"exit\n")
    sh.close()
    rep.check("yukari ok onceki satiri getirdi", recalled, sh.screen()[-300:])
    return sh.screen()


def case_sigint_discards_line(binary, rep):
    """Ctrl-C yarim satiri atar, kabuk yasar, sonraki satir tek basina islenir."""
    sh = Shell(binary)
    sh.ask(b"echo yarim satir")
    sh.wait_for("yarim satir")
    sh.send(b"\x03")
    sh.ask(b"echo sonrasi\n")
    sh.wait_for("sonrasi", skip=1)
    screen = sh.screen()
    sh.ask(b"exit\n")
    alive, info = sh.close()
    rep.check("Ctrl-C kabugu oldurmedi", not alive and "sinyal" not in info, info)
    rep.check(
        "Ctrl-C yarim satiri atti",
        "yarim satirsonrasi" not in screen,
        "iki satir birlesmis:\n" + screen[-300:],
    )
    rep.check("Ctrl-C sonrasi satir tek basina islendi",
              screen.count("sonrasi") >= 2, screen[-300:])
    rep.check("Ctrl-C ekranda ^C gosterdi", "^C" in screen, screen[-300:])
    return screen


def case_sigint_during_command(binary, rep):
    """Onplanda kosan komut Ctrl-C ile olur, kabuk yasar, durum 130 olur."""
    sh = Shell(binary)
    sh.ask(b"sleep 5\n")
    sh.quiet()
    sh.send(b"\x03")
    sh.ask(b"echo $?\n")
    saw = sh.wait_for("130")
    sh.ask(b"exit\n")
    alive, info = sh.close()
    rep.check("kosan komut Ctrl-C ile oldu, kabuk yasadi", not alive, info)
    rep.check("kesilen komutun durumu 130", saw, sh.screen()[-300:])
    return sh.screen()


def case_sigint_repeated(binary, rep):
    """Ust uste Ctrl-C her seferinde yeni prompt verir."""
    sh = Shell(binary)
    for _ in range(3):
        sh.ask(b"\x03")
    sh.ask(b"echo ayakta\n")
    survived = sh.wait_for("ayakta", skip=1)
    sh.ask(b"exit\n")
    alive, info = sh.close()
    rep.check("ust uste uc Ctrl-C sonrasi kabuk ayakta", survived and not alive, info)
    return sh.screen()


def case_sigquit_ignored(binary, rep):
    """Ctrl-\\ kabugu oldurmez ve yeni prompt uretmez."""
    sh = Shell(binary)
    sh.ask(b"\x1c")
    sh.send(b"echo ayakta\n")
    survived = sh.wait_for("ayakta", skip=1)
    sh.ask(b"exit\n")
    alive, info = sh.close()
    rep.check("Ctrl-\\ yok sayildi", survived and not alive, info)
    rep.check("Ctrl-\\ cekirdek dokumu birakmadi", "Quit" not in sh.screen(),
              sh.screen()[-200:])
    return sh.screen()


def case_fix_preload(binary, rep):
    """Yazim hatasinin duzeltilmisi bir sonraki prompta hazir geliyor."""
    sh = Shell(binary)
    sh.ask(b"ehco onyukleme-vakasi\n")
    suggested = sh.wait_for("bunu mu demek istediniz: echo")
    # Tamponda duzeltilmis satir gorunur: yazilan + hazirlanan = iki kez.
    preloaded = sh.wait_for("echo onyukleme-vakasi", skip=0)
    sh.send(b"\n")
    # Enter'dan sonra komut gercekten kosar; ciktisi ucuncu gorunum olur.
    ran = sh.wait_for("onyukleme-vakasi", skip=2)
    sh.ask(b"exit\n")
    alive, info = sh.close()
    rep.check("yazim hatasi icin oneri yazildi", suggested, sh.screen()[-300:])
    rep.check("oneri tampona hazir geldi", preloaded, sh.screen()[-300:])
    rep.check("Enter duzeltilmis komutu kostu", ran, sh.screen()[-300:])
    # Oneri BIR KEZ gelmeli. Kanca metni birakmazsa her promptta yeniden
    # yazilir ve sonraki "exit" satirinin basina eklenir; o zaman kabuk
    # hicbir zaman kapanmaz. Mutasyon denemesi bu boslugu gosterdi.
    rep.check("oneri yalnizca bir kez geldi", not alive, info)
    return sh.screen()


def case_fix_danger_not_preloaded(binary, rep):
    """
    Geri donusu olmayan komut ONERILIR ama tampona KONULMAZ.

    Tasarimin en onemli guvenlik kurali bu: tampona konan oneri tek
    Enter'la kosar. "chmdo x" icin "chmod x" hazirlanmis olsa refleks bir
    tus izinleri degistirirdi.
    """
    sh = Shell(binary)
    sh.ask(b"chmdo tehlike-vakasi\n")
    suggested = sh.wait_for("bunu mu demek istediniz: chmod")
    sh.quiet()
    # Tam satir yalniz tampona konulmussa ekranda gorunur; oneri satiri
    # komut adini yaziyor ama argumani yazmiyor.
    loaded = "chmod tehlike-vakasi" in sh.screen()
    sh.send(b"\n")
    sh.quiet()
    after = sh.screen()
    sh.ask(b"echo tehlike-sonrasi\n")
    alive = sh.wait_for("tehlike-sonrasi", skip=1)
    sh.ask(b"exit\n")
    sh.close()
    rep.check("riskli komut icin de oneri yazildi", suggested,
              sh.screen()[-300:])
    rep.check("riskli oneri tampona KONULMADI", not loaded,
              sh.screen()[-400:])
    rep.check("bos Enter hicbir sey kosturmadi",
              "chmod tehlike-vakasi" not in after, after[-400:])
    rep.check("riskli oneri sonrasi kabuk calismaya devam etti", alive,
              sh.screen()[-300:])
    return sh.screen()


def case_fix_history_ranking(binary, rep):
    """
    Esit uzaklikta iki aday varsa gecmiste kullanilan kazanir.

    KONTROLLU PATH SART: siralamayi olcmek icin ayni uzaklikta TAM iki
    aday gerekiyor; gercek PATH'te bunu garanti etmek mumkun degil.
    Fikstur iki calistirilabilir dosya kuruyor ve her oturumda birini
    kullaniyor. Iki oturum birlikte kanit oluyor: dizin okuma sirasi
    hangisini once verirse versin, oturumlardan biri gercekten siralamayi
    sinamis olur.

    HOME her oturumda ayri, cunku gecmis dosyasi HOME'dan tureiyor ve
    oturumlar birbirinin gecmisini gormemeli.
    """
    base = tempfile.mkdtemp(prefix="nax_pty_gecmis_")
    bindir = os.path.join(base, "bin")
    os.mkdir(bindir)
    for name in ("aaqqx1", "aaqqx2"):
        path = os.path.join(bindir, name)
        with open(path, "w") as handle:
            handle.write("#!/bin/sh\necho %s-kostu\n" % name)
        os.chmod(path, 0o755)

    screens = []
    won = {}
    for used in ("aaqqx1", "aaqqx2"):
        home = os.path.join(base, "home-" + used)
        os.mkdir(home)
        sh = Shell(binary, {"PATH": bindir + ":" + os.environ.get("PATH", ""),
                            "HOME": home})
        # Kullanilmayan aday gecmiste ARGUMAN olarak birkac kez geciyor.
        # Sayim basa bakmiyorsa bu satirlar onu one gecirir ve oturum
        # yanlis adayi onerir; mutasyon denemesi bu boslugu gosterdi.
        other = "aaqqx2" if used == "aaqqx1" else "aaqqx1"
        for _ in range(3):
            sh.ask(("echo %s\n" % other).encode())
            sh.quiet()
        sh.ask(("%s\n" % used).encode())
        sh.wait_for("%s-kostu" % used)
        sh.ask(b"aaqqx3\n")
        sh.wait_for("bunu mu demek istediniz:")
        sh.quiet()
        won[used] = ("bunu mu demek istediniz: %s" % used) in sh.screen()
        # Tamponda oneri hazir bekliyor; Ctrl-C ile atilmadan exit yazilsa
        # satirin basina eklenir ve kabuk kapanmaz.
        sh.send(b"\x03")
        sh.ask(b"exit\n")
        sh.close()
        screens.append(sh.screen())
    shutil.rmtree(base, ignore_errors=True)
    rep.check("esit uzaklikta gecmiste kullanilan aday kazanir",
              won["aaqqx1"] and won["aaqqx2"],
              "aaqqx1 kazandi=%s aaqqx2 kazandi=%s" % (won["aaqqx1"],
                                                       won["aaqqx2"]))
    return "".join(screens)


def case_fix_quoted_head_not_preloaded(binary, rep):
    """
    Bas satirin basinda aynen gecmiyorsa satir yeniden yazilmaz.

    NEDEN: token listesi satirdaki KONUMU tasimiyor. Tirnakli bir bas
    ("ehco" gibi) icin sozcugun metni ile satirdaki yazimi farkli
    uzunlukta; oneri korumasiz birlestirilirse tampona bozuk bir satir
    gelir. Olculdu: koruma kalkinca tampona "echoo\" selam" yaziliyor.
    """
    sh = Shell(binary)
    sh.ask(b'"ehco" selam\n')
    suggested = sh.wait_for("bunu mu demek istediniz: echo")
    sh.quiet()
    garbled = "echoo" in sh.screen()
    sh.ask(b"exit\n")
    alive, info = sh.close()
    rep.check("tirnakli bas icin de oneri yazildi", suggested,
              sh.screen()[-300:])
    rep.check("tirnakli basta bozuk satir tampona konulmadi", not garbled,
              sh.screen()[-300:])
    rep.check("tirnakli bas sonrasi kabuk duzgun kapandi", not alive, info)
    return sh.screen()


def case_eof(binary, rep):
    """Ctrl-D gercek dosya sonu olarak taninir."""
    sh = Shell(binary)
    sh.ask(b"\x04")
    saw_exit = sh.wait_for("exit")
    alive, info = sh.close()
    rep.check("Ctrl-D exit basip cikti", saw_exit and not alive, info)
    return sh.screen()


def main():
    binary = os.environ.get("NAX_BIN", "./nax")
    if not os.access(binary, os.X_OK):
        print("%sHATA%s: %s bulunamadi. Once 'make' kosun." % (RED, OFF, binary))
        return 1

    print("pty testleri (%s)" % binary)
    rep = Report()
    screens = []
    for case in (
        case_echo_and_exit,
        case_history,
        case_sigint_discards_line,
        case_sigint_during_command,
        case_sigint_repeated,
        case_sigquit_ignored,
        case_eof,
        case_fix_preload,
        case_fix_danger_not_preloaded,
        case_fix_history_ranking,
        case_fix_quoted_head_not_preloaded,
    ):
        screens.append(case(binary, rep))

    found = ""
    for screen in screens:
        found = sanitizer_report(screen)
        if found:
            break
    rep.check("etkilesimli yolda sizinti ve tanimsiz davranis yok",
              found == "", found)

    print("\n  %d gecti, %d patladi" % (rep.passed, rep.failed))
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
