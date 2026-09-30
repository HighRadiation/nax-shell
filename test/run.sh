#!/usr/bin/env bash
#
# nax test kosucusu.
#
# NEDEN VAR:
#   Hicbir kilometre tasi "testler yesil" olmadan bitmis sayilmaz. Bu betik
#   tek giris noktasidir; "make test" ve "make check" bunu cagirir.
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

# Denetleyicili ikiliyi sizinti raporu icin ayrica kosar.
run_leak_check() {
	local out

	if [ ! -x ./nax-asan ]; then
		printf "  ${S}atlandi${N} sizinti denetimi (nax-asan yok, 'make asan' gerekir)\n"
		return 0
	fi
	out="$(printf 'merhaba\nexit\n' | ASAN_OPTIONS=detect_leaks=1 ./nax-asan 2>&1 >/dev/null)"
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

run_case   "satiri geri yazar"          $'merhaba\n'            "merhaba"
run_case   "bos satir cikti uretmez"    $'\n\n\n'               ""
run_case   "bosluklu satir yok sayilir" $'   \t  \n'            ""
run_case   "exit ciktisiz ciker"        $'exit\n'               ""
run_case   "exit oncesi satir yazilir"  $'merhaba\nexit\n'      "merhaba"
run_case   "bosluklu exit de ciker"     $'  exit  \nsonra\n'    ""
run_status "temiz cikis kodu sifir"     $'merhaba\nexit\n'      0
run_status "EOF ile cikis kodu sifir"   $'merhaba\n'            0
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
