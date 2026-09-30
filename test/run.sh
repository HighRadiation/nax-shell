#!/usr/bin/env bash
#
# nax test kosucusu.
#
# NEDEN VAR:
#   Hicbir kilometre tasi "testler yesil" olmadan bitmis sayilmaz. Bu betik
#   tek giris noktasidir; "make test" ve "make check" bunu cagirir.
#
# UC TEST GRUBU, UCU AYRI SEYI GORUR:
#   birim testleri  modulleri dogrudan cagirir (NAX_UNITS ile verilir)
#   boru testleri   ikiliyi boru ile besler, etkilesimsiz yolu kosar
#   pty testleri    sahte terminalde etkilesimli yolu kosar
#
# NEDEN AYNI BETIK IKI IKILI ICIN:
#   valgrind bu ortamda yok. Bellek hatalari, denetleyicili ikili (nax-asan)
#   ayni vakalari kosarken yakalanir. Bu yuzden ikili disaridan verilebilir.
#
# Kullanim:
#   ./test/run.sh                    normal ikiliyi dener
#   NAX_BIN=./nax-asan ./test/run.sh denetleyicili ikiliyi dener

set -uo pipefail
cd "$(dirname "$0")/.."

G='\033[0;32m'
R='\033[0;31m'
S='\033[0;33m'
N='\033[0m'

BIN="${NAX_BIN:-./nax}"
PASS=0
FAIL=0

# Genisletme vakalarinin okudugu degiskenler. Birim testler kendi ortamini
# kuruyor; burada amac genisletmenin GERCEK ikilide de calistigini gormek.
export NAX_TV=deger
export NAX_TS="a b"

# Calistirma vakalari icin fikstur: kosan betik, kosmayan dosya, bir dizin.
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT
printf '#!/bin/sh\necho betik-kosdu\n' > "$FIX/kosar.sh"
chmod +x "$FIX/kosar.sh"
echo metin > "$FIX/kosmaz.txt"
chmod 644 "$FIX/kosmaz.txt"
mkdir "$FIX/birdizin"

# Tek bir vakayi kosar: girdiyi ikiliye verir, ciktiyi beklenenle karsilastirir.
run_case() {
	local name="$1" input="$2" want="$3" got

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 "$BIN" 2>/dev/null)"
	if [ "$got" = "$want" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   %s\n" "$name"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} %s\n" "$name"
		printf "    beklenen: %s\n" "$(printf '%q' "$want")"
		printf "    gelen   : %s\n" "$(printf '%q' "$got")"
	fi
}

# Girdiyi kosar ve yalnizca cikis kodunu karsilastirir.
run_status() {
	local name="$1" input="$2" want="$3" got

	printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 "$BIN" >/dev/null 2>&1
	got=$?
	if [ "$got" = "$want" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   %s\n" "$name"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} %s (beklenen kod %s, gelen %s)\n" "$name" "$want" "$got"
	fi
}

# Girdiyi kosar ve hata ciktisinda beklenen metni arar.
run_stderr() {
	local name="$1" input="$2" want="$3" got

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 "$BIN" 2>&1 >/dev/null)"
	case "$got" in
		*"$want"*)
			PASS=$((PASS + 1))
			printf "  ${G}gecti${N}   %s\n" "$name"
			;;
		*)
			FAIL=$((FAIL + 1))
			printf "  ${R}patladi${N} %s\n" "$name"
			printf "    beklenen icerik: %s\n" "$want"
			printf "    gelen          : %s\n" "$got"
			;;
	esac
}

# Cikti ve hatayi AYNI akista birlestirip karsilastirir; sira testi icin.
run_merged() {
	local name="$1" input="$2" want="$3" got

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 "$BIN" 2>&1)"
	if [ "$got" = "$want" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   %s\n" "$name"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} %s\n" "$name"
		printf "    beklenen: %s\n" "$(printf '%q' "$want")"
		printf "    gelen   : %s\n" "$(printf '%q' "$got")"
	fi
}

# Enum sabitlerimizin sistem makrolariyla cakismadigini dogrular.
#
# NEDEN VAR:
#   "R_OK" adli bir enum sabiti <unistd.h> icindeki access() makrosuyla
#   cakisti. Makro oldugu icin on islemci koddaki her R_OK metnini 4 ile
#   degistirdi, yani baska bir sabitin degeriyle. Ortaya gecerli kod
#   ciktigi icin derleyici TEK BIR UYARI vermedi; hata yalnizca davranista
#   gorundu ve bash ile karsilastirma testi yakaladi.
#
#   Bu denetim sinifi kapatiyor: basliklarimizdaki her buyuk harfli sabit
#   icin, sistem basliklarindan SONRA makro olarak tanimli mi diye bakar.
run_macro_clash_check() {
	local names bad n

	names=$(grep -hoE '^	[A-Z][A-Z0-9_]+' src/*.h | tr -d '\t' | sort -u)
	bad=""
	for n in $names; do
		printf '#include <unistd.h>\n#include <signal.h>\n#include <fcntl.h>\n#include <sys/stat.h>\n#include <sys/wait.h>\n#ifdef %s\n#error CAKISMA\n#endif\n' "$n" \
			| cc -fsyntax-only -xc - 2>/dev/null || bad="$bad $n"
	done
	if [ -z "$bad" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   sabitler sistem makrolariyla cakismiyor\n"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} sistem makrosuyla cakisan sabit:%s\n" "$bad"
	fi
}

# Denetleyicili ikiliyi sizinti raporu icin ayrica kosar.
run_leak_check() {
	local out

	if [ ! -x ./nax-asan ]; then
		printf "  ${S}atlandi${N} sizinti denetimi (nax-asan yok, 'make asan' gerekir)\n"
		return 0
	fi
	out="$(printf 'echo merhaba\nexit\n' | ASAN_OPTIONS=detect_leaks=1 ./nax-asan 2>&1 >/dev/null)"
	if [ -z "$out" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   sizinti ve tanimsiz davranis yok\n"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} denetleyici rapor uretti\n"
		printf '%s\n' "$out" | sed 's/^/    /' | head -25
	fi
}

if [ ! -x "$BIN" ]; then
	printf "${R}HATA${N}: %s bulunamadi. Once 'make' kosun.\n" "$BIN"
	exit 1
fi

# Birim testleri: tablo tabanli, kabuk ikilisini hic baslatmaz.
# Makefile bunlari NAX_UNITS icinde verir; elle kosuldugunda bos olabilir.
UNIT_RC=0
for unit in ${NAX_UNITS:-}; do
	if [ -x "$unit" ]; then
		"$unit" || UNIT_RC=1
		printf "\n"
	fi
done

printf "nax testleri (%s)\n" "$BIN"

run_case   "komut gercekten kosar"        $'echo merhaba\n'            "merhaba"
run_case   "argumanlar aktarilir"         $'printf "[%s]" a b c\n'     "[a][b][c]"
run_case   "tirnakli sozcuk tek arguman"  $'printf "[%s]" "bir iki"\n' "[bir iki]"
run_case   "degisken genisletilir"        $'echo $NAX_TV\n'            "deger"
run_case   "tirnaksiz deger bolunur"      $'printf "[%s]" $NAX_TS\n'   "[a][b]"
run_case   "tirnakli deger bolunmez"      $'printf "[%s]" "$NAX_TS"\n' "[a b]"
run_case   "ev dizini acilir"             $'printf "%s" ~\n'           "$HOME"
run_case   "bos satir cikti uretmez"      $'\n\n\n'                    ""
run_case   "exit ciktisiz ciker"          $'exit\n'                    ""
run_case   "bosluklu exit de ciker"       $'  exit  \nsonra\n'         ""
run_case   "tam yolla betik kosar"        "$FIX/kosar.sh"$'\n'    "betik-kosdu"

run_status "basarili komut kodu 0"        $'true\n'                    0
run_status "basarisiz komut kodu 1"       $'false\n'                   1
run_status "cikis kodu aktarilir"         $'sh -c "exit 37"\n'         37
run_status "sinyalle olum 128+sinyal"     $'sh -c "kill -TERM $$"\n'   143
run_status "olmayan komut kodu 127"       $'boylebirkomutyok\n'        127
run_status "olmayan yol kodu 127"         $'/yok/boyle/bir/yol\n'      127
run_status "dizin komut olarak kodu 126"  "$FIX/birdizin"$'\n'     126
run_status "calistirilamayan kodu 126"    "$FIX/kosmaz.txt"$'\n'   126
run_status "bos komut kodu 0"             $'$NAX_YOK\n'               0
run_status "sozdizimi hatasi kodu 2"      $'ls |\n'                   2

run_stderr "olmayan komut bildirilir"     $'boylebirkomutyok\n' "boylebirkomutyok: command not found"
run_stderr "dizin sebebi dogru"           "$FIX/birdizin"$'\n'   "Is a directory"
run_stderr "yetki sebebi dogru"           "$FIX/kosmaz.txt"$'\n' "Permission denied"
run_stderr "olmayan yol sebebi dogru"     $'/yok/boyle\n'       "No such file or directory"
run_stderr "boru hatasi bildirilir"       $'ls |\n'             "boru isaretinin iki yaninda"
run_stderr "kapanmamis tirnak bildirilir" "echo 'x"$'\n'       "kapanmamis tek tirnak"
run_stderr "desteklenmeyen operator"      $'a && b\n'           "desteklenmeyen operator"
# Belirsiz yonlendirme vakasi burada YOK: calistirici bu asamada
# yonlendirmeyi genisletmeden once reddediyor, yani hata mesaji hic
# olusmuyor. Birim testler (test/cases/expand.tsv) bunu kapsiyor; boru ve
# yonlendirme kostugunda buraya da gelecek.

run_case   "son durum sonraki satirda okunur" $'false\necho $?\n' "1"
run_case   "sozdizimi durumu okunur"          $'ls |\necho $?\n'  "2"

run_stderr "boru hatti henuz kosmuyor"    $'ls | wc\n'            "boru hatti bu asamada calistirilmiyor"
run_stderr "yonlendirme henuz kosmuyor"   $'echo a > /dev/null\n' "yonlendirme bu asamada calistirilmiyor"

run_merged "hata ve cikti dogru sirada"   $'echo bir\nls |\necho iki\n' \
           $'bir\nnax: boru isaretinin iki yaninda da komut olmali\niki'

run_macro_clash_check
run_leak_check

printf "\n  %d gecti, %d patladi\n" "$PASS" "$FAIL"

# Etkilesimli yol boru ile test edilemez; onu sahte terminal betigi dener.
PTY_RC=0
if command -v python3 >/dev/null 2>&1; then
	printf "\n"
	NAX_BIN="$BIN" ./test/pty_drive.py || PTY_RC=1
else
	printf "\n  ${S}atlandi${N} pty testleri (python3 yok)\n"
fi

[ "$FAIL" -eq 0 ] && [ "$PTY_RC" -eq 0 ] && [ "$UNIT_RC" -eq 0 ] || exit 1
