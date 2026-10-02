"""
pyharness.py - Python tarafindaki tablo tabanli testler icin iskele.

C tarafindaki test/harness.c'nin karsiligi. AYNI vaka dosyalarini
okuyabilmesi onemli: tel bicimi korpusu iki dilde de kosuluyor ve
boylece iki uygulamanin birbirinden sessizce ayrismasi mumkun olmuyor.

KACIS KURALLARI C ISKELESIYLE AYNI: "\\t" sekme, "\\e" ESC, "\\\\" ters bolu.
"\\n" BILINCLI OLARAK YOK - sozcuk ayirici korpusunda "\\n" gercek bir ters
bolu ile n demek ve onu cozmek o testlerin anlamini degistirirdi.
"""

import sys


def unescape(text):
    """Metindeki kacislari cozer; C iskelesiyle ayni kumeyi tanir."""
    out = []
    i = 0
    while i < len(text):
        if text[i] == "\\" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == "t":
                out.append("\t")
                i += 2
                continue
            if nxt == "e":
                out.append("\x1b")
                i += 2
                continue
            if nxt == "\\":
                out.append("\\")
                i += 2
                continue
        out.append(text[i])
        i += 1
    return "".join(out)


def read_cases(path):
    """Vaka dosyasini okur; (satir_no, girdi, beklenen) uretir."""
    with open(path, encoding="utf-8") as handle:
        for number, raw in enumerate(handle, 1):
            line = raw.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if "\t" not in line:
                continue
            given, want = line.split("\t", 1)
            yield number, unescape(given), unescape(want)


class Report:
    """Gecen ve patlayan vakalari sayar, ozet basar."""

    GREEN = "\033[0;32m"
    RED = "\033[0;31m"
    OFF = "\033[0m"

    def __init__(self, title):
        print(title)
        self.passed = 0
        self.failed = 0

    def ok(self):
        self.passed += 1

    def bad(self, number, given, want, got):
        self.failed += 1
        print("  %spatladi%s satir %d" % (self.RED, self.OFF, number))
        print("    girdi    : %s" % given)
        print("    beklenen : %s" % want)
        print("    gelen    : %s" % got)

    def judge(self, number, given, want, got):
        """Beklenenle geleni karsilastirir."""
        if got == want:
            self.ok()
        else:
            self.bad(number, given, want, got)

    def finish(self):
        """Ozeti basar ve kabuk cikis kodunu dondurur."""
        colour = self.GREEN if self.failed == 0 else self.RED
        print("  %s%d gecti%s, %d patladi"
              % (colour, self.passed, self.OFF, self.failed))
        return 1 if self.failed else 0


def main_for(path, title, runner):
    """Vakalari kosar ve cikis kodunu dondurur."""
    given_path = sys.argv[1] if len(sys.argv) > 1 else path
    report = Report("%s (%s)" % (title, given_path))
    for number, given, want in read_cases(given_path):
        runner(report, number, given, want)
    return report.finish()
