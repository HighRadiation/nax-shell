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
#   python birimleri yardimci surecin tarafi (test/unit_*.py)
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

# Her vaka zaman siniriyla kosar.
#
# NEDEN VAR: bir hata kabugu ASILDIRABILIR, ozellikle boru uclari
# kapatilmadiginda okuyan asama hic EOF gormez. Asilma hatadan KOTUDUR:
# suite patlamak yerine sessizce durur ve kimse sebebini gormez. Zaman
# siniri asilmayi normal bir vaka hatasina cevirir. Olculdu: ana surecte
# boru yazma ucunun kapatilmasi kaldirildiginda iki asamali bir hat bile
# asiliyor, cunku okuyan taraf hic EOF gormuyor.
CASE_TIMEOUT=10

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
printf 'satir1\nsatir2\n' > "$FIX/girdi.txt"
# Dosya adi genisletmesi fiksturu: iki ".g" dosyasi, bir gizli dosya, bir
# alt dizin ve adinda YILDIZ gecen bir dosya. Son dosya onemli: desen ile
# harf ayrimini olcen tek sey o.
mkdir "$FIX/glob"
: > "$FIX/glob/bir.g"
: > "$FIX/glob/iki.g"
: > "$FIX/glob/uc.txt"
: > "$FIX/glob/.gizli"
: > "$FIX/glob/yildiz*dosya"
mkdir "$FIX/glob/alt"
: > "$FIX/glob/alt/ic.g"

mkdir "$FIX/saltokunur"
chmod 555 "$FIX/saltokunur"

# Tek bir vakayi kosar: girdiyi ikiliye verir, ciktiyi beklenenle karsilastirir.
run_case() {
	local name="$1" input="$2" want="$3" got

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 timeout "$CASE_TIMEOUT" "$BIN" 2>/dev/null)"
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

	printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 timeout "$CASE_TIMEOUT" "$BIN" >/dev/null 2>&1
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
# Bir metnin stderr'de GECMEDIGINI dogrular.
#
# NEDEN GEREKLI: bazi kurallar bir seyin YAPILMAMASI. "Calistirilamayan
# dosya onerilmez" kurali, cikan mesaji degil cikmayan adi olcmek
# demek; "icerir" bicimindeki yardimcilarla bu yazilamiyor.
# Iki maskeyi karsilastirip sonucu basar.
report_mask() {
	local name="$1" want="$2" got="$3"

	if [ "$got" = "$want" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   %s\n" "$name"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} %s\n" "$name"
		printf "    beklenen: %s\n" "$want"
		printf "    gelen   : %s\n" "$got"
	fi
}

# Cocuk, kabugun DEVRALDIGINDAN fazlasini gormemeli.
#
# BEKLENEN MASKE SABIT YAZILAMAZ. Testleri baslatan surec zinciri bazi
# sinyalleri yok sayiyor ve bu zincir ortama gore degisiyor; olculdu,
# "make" altinda maske 0x180000000, dogrudan kosumda 0. Ilk yazimda sifir
# bekleniyordu ve "make check" altinda patladi - kodda bir sorun
# olmadigi halde.
#
# Dogru soru mutlak deger degil: ayni sekilde baslatilan DOGRUDAN bir
# cocuk ile nax'in cocugu AYNI maskeyi gormeli. Fark varsa kabuk bir sey
# ekliyor ya da devralinani kaybediyor demektir.
run_signal_mask_check() {
	local want got

	want="$(timeout "$CASE_TIMEOUT" grep SigIgn /proc/self/status)"
	got="$(printf 'grep SigIgn /proc/self/status\n' \
		| timeout "$CASE_TIMEOUT" "$BIN" 2>/dev/null)"
	report_mask "cocuk fazladan yok sayilan sinyal gormuyor" "$want" "$got"
}

# Devralinan yok sayma korunur.
#
# "trap '' PIPE" alt kabukta SIGPIPE'i yok sayili yapar ve nax bunu
# devralir. Olcum yine goreli: iki taraf da AYNI tuzak altinda olculuyor,
# yani beklenen maske SIGPIPE bitini kendiliginden iceriyor.
#
# Bu vaka "her seyi varsayilana dondur" yaklasimini reddediyor; bash da
# devralinani koruyor (olculdu).
run_inherited_ignore_check() {
	local want got

	want="$(trap '' PIPE; timeout "$CASE_TIMEOUT" grep SigIgn /proc/self/status)"
	got="$(trap '' PIPE; printf 'grep SigIgn /proc/self/status\n' \
		| timeout "$CASE_TIMEOUT" "$BIN" 2>/dev/null)"
	report_mask "devralinan yok sayma cocuga gecer" "$want" "$got"
}

# Bir metnin STDOUT'ta gecmedigini dogrular.
#
# Gizlilik vakalari icin: "nax ctx" ciktisinda sirrin GORUNMEMESI
# olculuyor. docs/PRIVACY.md bunu acikca testin isi sayiyor.
# Stdout'un verilen metni ICERDIGINI dogrular.
run_case_contains() {
	local name="$1" input="$2" want="$3" got

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 timeout "$CASE_TIMEOUT" "$BIN" 2>/dev/null)"
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

run_out_absent() {
	local name="$1" input="$2" unwanted="$3" got

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 timeout "$CASE_TIMEOUT" "$BIN" 2>/dev/null)"
	case "$got" in
		*"$unwanted"*)
			FAIL=$((FAIL + 1))
			printf "  ${R}patladi${N} %s\n" "$name"
			printf "    cikmamasi gereken: %s\n" "$unwanted"
			;;
		*)
			PASS=$((PASS + 1))
			printf "  ${G}gecti${N}   %s\n" "$name"
			;;
	esac
}

run_stderr_absent() {
	local name="$1" input="$2" unwanted="$3" got

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 timeout "$CASE_TIMEOUT" "$BIN" 2>&1 >/dev/null)"
	case "$got" in
		*"$unwanted"*)
			FAIL=$((FAIL + 1))
			printf "  ${R}patladi${N} %s\n" "$name"
			printf "    cikmamasi gereken: %s\n" "$unwanted"
			printf "    gelen            : %s\n" "$got"
			;;
		*)
			PASS=$((PASS + 1))
			printf "  ${G}gecti${N}   %s\n" "$name"
			;;
	esac
}
run_stderr() {
	local name="$1" input="$2" want="$3" got

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 timeout "$CASE_TIMEOUT" "$BIN" 2>&1 >/dev/null)"
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

	got="$(printf '%s' "$input" | ASAN_OPTIONS=detect_leaks=1 timeout "$CASE_TIMEOUT" "$BIN" 2>&1)"
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

	names=$(grep -hoE '^	[A-Z][A-Z0-9_]+' src/*.h src/*/*.h | tr -d '\t' | sort -u)
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

# Boru uclarinin kapatildigini dogrular.
#
# NEDEN VAR: bir asama, kendi cikis borusunun OKUMA ucunu da devralir.
# Kapatilmazsa iki sey olur: fd'ler birikir, ve daha kotusu okuyan asama
# hic EOF gormedigi icin asili kalir. Iki yuz hat kosup son komutun hala
# calistigini dogrulamak bu sinifi yakalar.
run_leak_fd_check() {
	local script out

	script=$(for _ in $(seq 200); do printf 'printf "a\\nb\\n" | wc -l\n'; done)
	out=$(printf '%s\nprintf SONKOMUT\n' "$script" | timeout 60 "$BIN" 2>&1 | tail -1)
	if [ "$out" = "SONKOMUT" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   200 boru hatti sonrasi kabuk saglam\n"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} 200 boru hatti sonrasi son komut kosmadi\n"
		printf "    gelen: %s\n" "$out"
	fi
}

# Kod normlarini dogrular.
#
# NEDEN SUITE ICINDE:
#   Bu denetimler her kilometre tasinda ELLE kosuluyordu ve bir kez sessizce
#   daraldilar: kaynaklar alt klasorlere tasinirken "src/*.h" deseni yalnizca
#   tek basligi bulmaya basladi, yani makro cakisma denetimi HICBIR SEY
#   denetlemedigi halde yesil gectii. Elle kosulan denetim, kosulmayan
#   denetime donusur. Suite icinde olunca bu olamaz.
#
# Kurallar docs/NORMS.md icinde yazili; buradaki her kontrol oradaki bir
# kurala karsilik geliyor.
# Ayni tip adinin iki ayri yapi icin kullanilmadigini dogrular.
#
# NEDEN VAR: proto.h bir anahtar/deger cifti icin t_field tanimlamisti,
# oysa parse.h genisletmenin urettigi alan icin ayni adi kullaniyordu.
# Iki baslik ilk kez bir arada kullanilana kadar hata gorunmedi - yani
# aylarca sessizce bekleyebilirdi.
#
# R_OK dersinin aynisi: bulunan hatayi tek tek degil SINIF olarak kapat.
run_type_clash_check() {
	local dupes

	dupes=$(grep -hoE '^typedef (struct|enum|union) (s|e|u)_[a-z0-9_]+' \
		src/*.h src/*/*.h test/*.h 2>/dev/null \
		| awk '{print $NF}' | sort | uniq -d)
	if [ -z "$dupes" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   tip adlari cakismiyor\n"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} ayni ad iki yapi icin kullanilmis:%s\n" \
			"$(echo $dupes)"
	fi
}

run_norm_check() {
	local src hdr bad

	src=$(ls src/*.c src/*/*.c test/*.c 2>/dev/null)
	hdr=$(ls src/*.h src/*/*.h test/*.h 2>/dev/null)
	bad=""
	grep -qE '^[[:blank:]]+(/\*|//)' $src $hdr && bad="$bad fonksiyon-icinde-yorum"
	grep -qE 'return[[:blank:]]+[^(;]' $src && bad="$bad parantezsiz-return"
	grep -qE '^[a-zA-Z_].*\)[[:blank:]]*\{' $src && bad="$bad ayni-satirda-susly"
	grep -qP '^ ' $src $hdr && bad="$bad bosluk-girinti"
	grep -qP '[^\x00-\x7F]' $src $hdr && bad="$bad kodda-ascii-disi-karakter"
	grep -qzoP 'typedef\s+(struct|enum|union)\s+\w*\s*\{' $src \
		&& bad="$bad c-dosyasinda-yapi-tanimi"
	if [ -z "$bad" ]; then
		PASS=$((PASS + 1))
		printf "  ${G}gecti${N}   kod normlari\n"
	else
		FAIL=$((FAIL + 1))
		printf "  ${R}patladi${N} norm ihlali:%s\n" "$bad"
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

# Python birim testleri: yardimci surecin kendi tarafi ve tel biciminin
# iki dilde ayni olmasi. Makefile'dan gelmiyorlar cunku derlenmiyorlar;
# dogrudan calistirilabilir dosyalar.
for unit in test/unit_*.py; do
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
# "a && b" ARTIK DESTEKLENIYOR; buradaki vaka halen desteklenmeyen bir
# yapiya tasindi. Artalan isareti ve parantez vakalari asagida.
run_stderr "parantez desteklenmiyor"      $'( a )\n'              "desteklenmeyen operator"
# Belirsiz yonlendirme vakasi burada YOK: calistirici bu asamada
# yonlendirmeyi genisletmeden once reddediyor, yani hata mesaji hic
# olusmuyor. Birim testler (test/cases/expand.tsv) bunu kapsiyor; boru ve
# yonlendirme kostugunda buraya da gelecek.

run_case   "son durum sonraki satirda okunur" $'false\necho $?\n' "1"
run_case   "sozdizimi durumu okunur"          $'ls |\necho $?\n'  "2"


run_case   "iki asamali boru"             $'printf "a\\nb\\nc\\n" | wc -l\n'      "3"
run_case   "uc asamali boru"              $'printf "a\\nb\\nc\\n" | grep -v b | wc -l\n' "2"
run_case   "dort asamali boru"            $'printf "c\\na\\nb\\n" | sort | head -2 | tr "\\n" ","\n' "a,b,"
run_case   "yazan taraf erken kapanir"    $'yes | head -2 | tr "\\n" ","\n'  "y,y,"
run_status "boru kodu son komuttan (0)"   $'false | true\n'       0
run_status "boru kodu son komuttan (1)"   $'true | false\n'       1
run_status "son komut bulunamaz"          $'echo x | boylebirkomutyok\n' 127
run_status "ilk komut bulunamaz"          $'boylebirkomutyok | wc -l\n'  0

run_case   "dosyaya yaz ve geri oku"      "echo icerik > $FIX/o1"$'\n'"cat $FIX/o1"$'\n' "icerik"
run_case   "dosyadan oku"                 "wc -l < $FIX/girdi.txt"$'\n' "2"
# Cocuklara fd sizmadigini dogrular. Boru asamasi kendi cikis borusunun
# okuma ucunu devralir; kapatilmazsa calistirilan programa sizar. Linux'a
# ozgu bir kontrol (/proc), bu proje Linux'ta kosuyor.
run_case   "cocuga fazla fd sizmaz"       $'ls /proc/self/fd | tr -d "\\n"\n' "0123"
run_case   "ekleme ustune yazmaz"         "echo bir > $FIX/ek"$'\n'"echo iki >> $FIX/ek"$'\n'"cat $FIX/ek"$'\n' $'bir\niki'
run_case   "cikti ve girdi birlikte"      "wc -l < $FIX/girdi.txt > $FIX/o2"$'\n'"cat $FIX/o2"$'\n' "2"
run_case   "boru ciktisi dosyaya"         'printf "a\nb\n" | wc -l > '"$FIX"'/o3'$'\n'"cat $FIX/o3"$'\n' "2"
run_case   "yonlendirme boruyu ezer"      "echo veri > $FIX/o4 | wc -c"$'\n'"cat $FIX/o4"$'\n' $'0\nveri'
run_case   "sadece > dosya olusturur"     "> $FIX/o5"$'\n'"wc -c < $FIX/o5"$'\n' "0"

run_status "okunamayan dosyadan oku"      "cat < $FIX/saltokunur/yok"$'\n' 1
run_status "yazilamayan dizine yaz"       "echo x > $FIX/saltokunur/yeni"$'\n' 1
run_status "olmayan dizine yaz"           $'echo x > /yok/dizin/dosya\n' 1
run_stderr "yonlendirme sebebi bildirilir" $'cat < /yok/boyle/dosya\n' "No such file or directory"
run_stderr "belirsiz yonlendirme"         $'cat > $NAX_TS\n'      "belirsiz yonlendirme"

# fd sizintisi gerilemesi: 200 boru hatti kosup son komutun hala
# calistigini dogrular. Boru uclari kapatilmazsa hem fd tukenir hem de
# okuyan asama EOF gormedigi icin asilir.
run_leak_fd_check

run_merged "hata ve cikti dogru sirada"   $'echo bir\nls |\necho iki\n' \
           $'bir\nnax: boru isaretinin iki yaninda da komut olmali\niki'

run_case   "cd ve pwd"                    "cd $FIX"$'\n'"pwd"$'\n' "$FIX"
run_case   "cd sonrasi PWD guncellenir"   "cd $FIX"$'\n''echo $PWD'$'\n' "$FIX"
run_case   "cd sonrasi OLDPWD guncellenir" "cd $FIX"$'\n''echo $OLDPWD'$'\n' "$PWD"
run_case   "cd - geri doner"              "cd $FIX"$'\n'"cd -"$'\n' "$PWD"
run_status "cd olmayan dizin kodu 1"      $'cd yokboylebirdizin\n'      1
run_stderr "cd sebebi dogru"              $'cd yokboylebirdizin\n'      "cd: yokboylebirdizin: No such file or directory"
run_stderr "cd dosyaya sebebi dogru"      "cd $FIX/girdi.txt"$'\n'      "Not a directory"
run_stderr "cd fazla arguman"             $'cd a b\n'                   "cd: too many arguments"

run_case   "echo -n yeni satir yazmaz"    $'echo -n abc\necho SON\n'   "abcSON"
run_case   "echo -nn de bayrak"           $'echo -nn abc\necho SON\n'  "abcSON"
run_case   "echo -n-n bayrak degil"       $'echo -n-n abc\n'            "-n-n abc"
run_case   "echo argumansiz bos satir"    $'echo\necho SON\n'          $'\nSON'

run_case   "export ve genisletme"         $'export NAXX=deger\necho $NAXX\n' "deger"
run_case   "unset siler"                  $'export NAXX=1\nunset NAXX\necho "[$NAXX]"\n' "[]"
run_status "export gecersiz ad kodu 1"    $'export 1X=1\n'              1
run_stderr "export gecersiz ad sebebi"    $'export 1X=1\n'              "not a valid identifier"
run_status "unset gecersiz ad kodu 0"     $'unset 1X\n'                 0

run_status "exit 7"                       $'exit 7\n'                   7
# "exit 7" tek basina yazildiginda kabuk zaten EOF ile cikiyor, yani cikis
# kodu bayragin kurulup kurulmadigini olcmez. Ardindan bir komut gelmesi
# sart; mutasyon denemesi bu boslugu gosterdi.
run_case   "exit 7 kabuktan CIKAR"        $'exit 7\necho DEVAM\n'      ""
run_status "exit -44 mod 256"             $'exit -44\n'                 212
run_status "exit son durumu kullanir"     $'false\nexit\n'             1
run_status "exit 300 mod 256"             $'exit 300\n'                 44
run_status "exit -1 mod 256"              $'exit -1\n'                  255
run_status "exit abc kodu 2"              $'exit abc\n'                 2
run_case   "exit abc kabuktan CIKAR"      $'exit abc\necho DEVAM\n'    ""
run_case   "exit 1 2 kabuktan CIKMAZ"     $'exit 1 2\necho DEVAM\n'    "DEVAM"

# Ana surecte kosan yerlesik yonlendirmeyi uyguluyor VE kabugun kendi
# ciktisi bozulmadan geri yukleniyor. Ikinci kisim kritik: fflush
# yapilmadan geri yuklenirse tamponda bekleyen cikti yanlis yere gider.
run_case   "yerlesik yonlendirmesi"       "pwd > $FIX/p1"$'\n'"cat $FIX/p1"$'\n' "$PWD"
run_case   "yerlesikten sonra stdout saglam" "pwd > $FIX/p2"$'\n'"echo SONRA"$'\n' "SONRA"

# Boru hattindaki yerlesik COCUKTA kosar, yani kabugun durumunu
# degistirmez; bash de boyle davraniyor (olculdu).
run_case   "boru icindeki cd kabugu etkilemez" "cd $FIX"$'\n'"echo x | cd /"$'\n'"pwd"$'\n' "$FIX"
run_case   "boru icindeki echo kosar"     $'echo merhaba | cat\n'       "merhaba"
run_case   "boru icindeki export etkisiz" $'echo x | export NAXY=1\necho "[$NAXY]"\n' "[]"
# Yukaridaki vaka yerlesigin TANINDIGINI olcemez: tanmnmazsa da degisken
# ayarlanmaz. export'un harici karsiligi olmadigi icin taninmadiginda
# "command not found" cikar; bu yuzden birlesik cikti bos olmak zorunda.
# Mutasyon denemesi bu boslugu gosterdi.
run_merged "boru icinde yerlesik taninir" $'echo x | export NAXY=1\n'   ""

# --- dosya adi genisletmesi ---
# Davranisin tamami bash ile karsilastirildi; farklarin hepsi giderildi.
run_case   "yildiz eslesenleri verir"     "cd $FIX/glob && echo *.g"$'\n' "bir.g iki.g"
# SONUC SIRALI: dizin okuma sirasi dosya sistemine gore degisir, ayni
# komutun farkli siralarda cikti vermesi kabul edilemez.
run_case   "sonuc sirali"                 "cd $FIX/glob && echo *"$'\n' "alt bir.g iki.g uc.txt yildiz*dosya"
# TIRNAKLI YILDIZ DESEN DEGIL: adinda yildiz gecen dosya ile eslesiyor.
run_case   "tirnakli yildiz harf olur"    "cd $FIX/glob && echo \"yildiz*dosya\""$'\n' "yildiz*dosya"
run_case   "kacisli yildiz harf olur"     "cd $FIX/glob && echo yildiz\\*dosya"$'\n' "yildiz*dosya"
# Tirnaksiz GENISLETME SONUCU desen olur; bu bash'te boyle ve olculdu.
run_case   "degisken degeri desen olur"   "cd $FIX/glob && export P=*.g && echo \$P"$'\n' "bir.g iki.g"
run_case   "tirnakli degisken harf olur"  "cd $FIX/glob && export P=*.g && echo \"\$P\""$'\n' "*.g"
# Soru isareti ve kume.
# Tek karakterli ".g" dosyasi yok, yani eslesme olmuyor ve desen kaliyor.
run_case   "soru isareti tek karakter"    "cd $FIX/glob && echo ?.g"$'\n' "?.g"
run_case   "soru isareti eslesirse gelir" "cd $FIX/glob && echo ???.g"$'\n' "bir.g iki.g"
run_case   "kume eslesir"                 "cd $FIX/glob && echo [bi]*.g"$'\n' "bir.g iki.g"
# GIZLI DOSYA yalnizca desen nokta ile baslarsa eslesir.
run_case   "gizli dosya yildizla gelmez"  "cd $FIX/glob && echo *i*"$'\n' "bir.g iki.g yildiz*dosya"
run_case   "gizli dosya nokta ile gelir"  "cd $FIX/glob && echo .giz*"$'\n' ".gizli"
# "." ve ".." hicbir desenle eslesmez.
run_case   "nokta girdileri gelmez"       "cd $FIX/glob && echo .*"$'\n' ".gizli"
# Bilesen bazinda: yildiz bolu isaretini gecmiyor.
run_case   "alt dizinde desen"            "cd $FIX/glob && echo alt/*.g"$'\n' "alt/ic.g"
run_case   "yildiz bolu gecmez"           "cd $FIX/glob && echo */ic.g"$'\n' "alt/ic.g"
# ESLESME YOKSA DESEN OLDUGU GIBI KALIR: alani silmek kullanicinin
# yazdigini sessizce yok etmek olurdu.
run_case   "eslesme yoksa desen kalir"    "cd $FIX/glob && echo yok*.zzz"$'\n' "yok*.zzz"
# Yonlendirme hedefi de genisletiliyor; birden fazla eslesme belirsiz.
run_case   "yonlendirme hedefi genisler"  "cd $FIX/glob && echo ic > bir.g && cat bir*.g"$'\n' "ic"
run_stderr "cok eslesen hedef belirsiz"   "cd $FIX/glob && echo x > *.g"$'\n' "belirsiz yonlendirme"

# --- here-document ---
# Govde ayristirmadan SONRA toplaniyor: kabugun girdi yolu bir satirdan
# fazlasini okuyor. Davranisin tamami bash ile karsilastirildi.
run_case   "govde girdiye verilir"        $'cat << SON\nbir\niki\nSON\n' "bir
iki"
# Sinirlayici TAM eslesmeli; "SONX" satiri "SON"u kapatmaz.
run_case   "sinirlayici tam eslesir"      $'cat << SON\nSONX\nSON\n' "SONX"
# Tirnaksiz sinirlayici: govdedeki degisken genisletilir.
run_case   "tirnaksiz sinirlayici genisletir" $'export A=dunya\ncat << SON\nmerhaba $A\nSON\n' "merhaba dunya"
# Tirnakli sinirlayici: govde HARFI HARFINE gider. Kullanicinin "$" iceren
# bir metni aynen gondermesinin tek yolu bu.
# Beklenen deger TEK TIRNAK icinde: run.sh kendi icinde genisletmesin.
run_case   "tirnakli sinirlayici harfi harfine" \
	$'export A=dunya\ncat << "SON"\nmerhaba $A\nSON\n' 'merhaba $A'
# Bos govde gecerli.
run_case   "bos govde gecerli"            $'cat << SON\nSON\necho bitti\n' "bitti"
# Boru hattinda da calisir.
run_case   "boru hattinda govde"          $'cat << SON | tr a-z A-Z\nkucuk\nSON\n' "KUCUK"
# Birden fazla govde YAZILDIKLARI SIRADA okunur; son yonlendirme kazanir.
run_case   "cok govdede sira korunur"     $'cat << A << B\natilan\nA\nkullanilan\nB\n' "kullanilan"
# Dosya sonu gelirse toplanan kadari kullanilir ve uyari basilir; bash da
# boyle yapiyor. Satiri tamamen reddetmek yazilan her seyi kaybettirirdi.
run_stderr "dosya sonu uyari verir"       $'cat << SON\nyarim\n' "dosya sonuyla kesildi"
run_case   "dosya sonunda toplanan kullanilir" $'cat << SON\nyarim\n' "yarim"

# --- boru hatti listesi: ; && || ---
# Anlamlarin tamami bash ile karsilastirilarak olculdu.
run_case   "noktali virgul ikisini de kosar" $'echo bir ; echo iki\n' "bir
iki"
run_case   "ve baglantisi basariliysa kosar" $'true && echo gorundu\n' "gorundu"
run_case   "ve baglantisi basarisizsa atlar" $'false && echo gorunmez\n' ""
run_case   "veya baglantisi basarisizsa kosar" $'false || echo yedek\n' "yedek"
run_case   "veya baglantisi basariliysa atlar" $'true || echo gorunmez\n' ""
# ATLANAN HATTIN DURUMU DEGISMEZ: kosan son sey false oldugu icin 1.
# Bash da boyle davraniyor (olculdu).
run_case   "atlanan hat durumu degistirmez"  $'false && true ; echo $?\n' "1"
run_case   "zincir soldan saga degerlendirilir" $'false || echo a && echo b\n' "a
b"
# Baglanti boru hattini boler, komutu degil.
run_case   "boru ve baglanti birlikte"       $'printf "x\\ny\\n" | wc -l && echo bitti\n' "2
bitti"
# Yonlendirme hattin kendi komutuna ait kaliyor.
run_case   "liste icinde yonlendirme"        "echo ic > $FIX/l1 && cat $FIX/l1"$'\n' "ic"
# Hatali liste anlasilir hata veriyor ve durum 2 oluyor.
run_stderr "operator yaninda komut yoksa"    $'echo a &&\n' "operatorun iki yaninda da komut olmali"
run_status "liste sozdizimi hatasi durumu 2" $'echo a &&\n' 2
# Artalan isareti ve parantez halen desteklenmiyor; anlasilir mesaj.
run_stderr "artalan isareti desteklenmiyor"  $'sleep 1 &\n' "desteklenmeyen operator"

# --- baglam ve gizlilik ---
# "nax ctx" bir sonraki istekte gidecek baytlari basar. Gizlilik iddiasini
# denetlenebilir kilan tek ozellik bu: belgeye guvenmek zorunda degilsin.
run_case_contains "ctx kosan komutu gosterir" $'echo kanit\nctx\n' "[0] echo kanit"
# SAHTE SIR EKLENIP GORUNMEDIGI DOGRULANIYOR. PRIVACY.md bunu acikca
# testin isi sayiyor.
run_out_absent "ctx sirri sizdirmaz" \
	$'export K=gsk_abcdefghij1234567890ABCDEFGHIJ\nctx\n' \
	"gsk_abcdefghij"
run_case_contains "ctx sirrin yerine yer tutucu koyar" \
	$'export K=gsk_abcdefghij1234567890ABCDEFGHIJ\nctx\n' "[GIZLI:anahtar]"
# Bosluk ile baslayan satir baglama HIC girmez.
run_out_absent "bosluklu satir baglama girmez" \
	$'echo bir\n  gizli-kalsin\nctx\n' "gizli-kalsin"
# Ortam degiskenlerinin DEGERLERI gonderilmez; yalnizca adlar.
run_out_absent "ortam degeri gonderilmez" $'ctx\n' "$NAX_TV"
run_case_contains "ortam adi gonderilir" $'ctx\n' "NAX_TV"

# --- elle yazilmis niyet tablosu ---
# Tablo MODELDEN ONCE bakiliyor ve eslesirse yardimci surec HIC
# baslatilmiyor. Bunu olcmek icin AI kasten bozuk: baslatilmaya calisilsa
# "duz kabuk" uyarisi cikardi, cikmiyorsa hic denenmemis demektir.
export NAX_NAXD="python3 test/fake_naxd.py die_at_start"
run_stderr "tablo eslesmesi komut uretir" $'disk kullanimini goster\n' "nax: df -h"
run_stderr_absent "tablo eslesirse AI denenmez" $'disk kullanimini goster\n' "duz kabuk"
run_status "tablo eslesmesi durum 0"      $'disk kullanimini goster\n' 0
# Turkce karakterli yazim da eslesir.
run_stderr "turkce karakterli yazim"      $'bellek kullan\xc4\xb1m\xc4\xb1\n' "nax: free -h"
# Tabloda olmayan istek modele gider; AI bozuk oldugu icin duz kabuk olur.
run_stderr "tabloda yoksa modele gider"   $'sunucuya baglan\n' "duz kabuk"
unset NAX_NAXD

# --- AI hatti ---
# VAKALARDA KULLANILAN CUMLE TABLOYA TAKILMAMALI: "dun degisen dosyalari
# goster" artik elle yazilmis tabloda ve modele hic gitmiyor. Bu yuzden
# AI vakalari tabloda OLMAYAN bir istek kullaniyor.
# Yardimci surec NAX_NAXD ile secilir; testler taklit surece yoneltiyor.
# Gercek surece yoneltmek aga cikmak, anahtar ve ucret demek olurdu ve
# ustelik ayni girdi her zaman ayni cevabi vermezdi.
export NAX_NAXD="python3 test/fake_naxd.py ok"
# Niyet komuta cevrilir ve oneri yazilir.
run_stderr "niyet oneri uretir"           $'eski loglari sil\n' "nax: ls -la"
run_status "oneri sonrasi durum 0"        $'eski loglari sil\n' 0
# Betik kipinde oneri KOSMAZ; yalnizca yazilir.
run_case   "AI onerisi kendiliginden kosmaz" $'eski loglari sil\n' ""
# Son hata aciklanabilir; cevap ekrana basilir.
run_case   "son hata aciklanir"           $'boylebirkomutyok\n?\n' "komut bulunamadi"
# Son komut basariliysa sorulmaz: basarili bir komutu "neden patladi" diye
# sormak modelden uydurma cevap almak demek, ustelik bedava degil.
run_stderr "basarili komut aciklanmaz"    $'echo x\n?\n' "aciklanacak hata yok"
# Hic komut kosmadan "?" yazilirsa sessiz kalinmaz.
run_stderr "aciklanacak komut yoksa soylenir" $'?\n' "aciklanacak bir komut yok"
unset NAX_NAXD

# Soru ile istek ayrimi: model SORU cevabi dondurdugunde metin ekrana
# basilir, duzenleme satirina konmaz.
export NAX_NAXD="python3 test/fake_naxd.py answer"
run_case   "soru cevabi ekrana basilir"   $'eski loglari sil\n' "bu bir dizin listesi"
unset NAX_NAXD

# AI baslatilamazsa kabuk DUZ KABUK olur: sebep bir kez yazilir, satir
# bash'in yaptigi seye duser (ilk sozcuk icin "command not found", 127).
export NAX_NAXD="python3 test/fake_naxd.py die_at_start"
run_stderr "AI yoksa sebep yazilir"       $'eski loglari sil\n' "duz kabuk"
run_stderr "AI yoksa komut bulunamadi"    $'eski loglari sil\n' "eski: command not found"
run_status "AI yoksa durum 127"           $'eski loglari sil\n' 127
unset NAX_NAXD

# Zaman asimi EL SIKISMADA bildirilen sureden gelir. Taklit READY'de bir
# saniye bildiriyor; ontanimli on bes saniye uygulanirsa bu vaka
# zaman asimina ugrar ve patlar.
export NAX_NAXD="python3 test/fake_naxd.py silent_fast"
run_stderr "cevap gelmezse zaman asimi"   $'eski loglari sil\n' "AI cevap vermedi"
run_status "zaman asimi sonrasi durum 1"  $'eski loglari sil\n' 1
unset NAX_NAXD

# Bozuk konusan surec ihlal sayilir ve baglanti kapatilir.
export NAX_NAXD="python3 test/fake_naxd.py garbage"
run_stderr "bozuk cevap baglantiyi keser" $'eski loglari sil\n' "baglantisi koptu"
unset NAX_NAXD

# AI'in kapali oldugu dizinde HICBIR SEY gonderilmez. Karar gonderen
# tarafta oldugu icin veri karsi tarafa hic ulasmiyor; kullaniciya tek
# satir bildirilir ve satir duz kabuk gibi ele alinir.
export NAX_NAXD="python3 test/fake_naxd.py nogo"
export NAX_FAKE_NOGO="$PWD"
run_stderr "kapali dizinde AI calismaz"   $'eski loglari sil\n' "bu dizinde AI kapali"
run_status "kapali dizinde durum 127"     $'eski loglari sil\n' 127
# Liste baska bir dizini gosteriyorsa istek normal gider.
export NAX_FAKE_NOGO="/kesinlikle-olmayan-bir-dizin"
run_stderr "liste disinda AI calisir"     $'eski loglari sil\n' "nax: ls -la"
unset NAX_FAKE_NOGO
unset NAX_NAXD

# --- cocuga sizan sinyal davranisi ---
run_signal_mask_check
run_inherited_ignore_check
# Yazan taraf kapanan boruda gercekten oluyor; hat asili kalmiyor.
run_case   "kapanan boruda yazan taraf olur"  $'yes | head -1\n' "y"

# --- yerel yazim duzeltmesi ---
# Vakalar YERLESIK yazim hatasi kullaniyor: yerlesikler PATH icerigine
# bagli olmadigi icin sonuc her ortamda ayni.
run_stderr "yazim hatasi oneri veriyor"   $'ehco selam\n' "bunu mu demek istediniz: echo"
# Mesaj BASIN kendisini yazmali, tum satiri degil; bash da boyle yapiyor.
run_stderr "hata mesaji basi adlandirir"  $'ehco selam\n' "ehco: boyle bir komut yok"
# Hicbir sey kosmadi, yani dogru kod 127. Oneri yazilmis olmasi bunu
# degistirmiyor. Mutasyon denemesi bu boslugu gosterdi.
run_status "oneri sonrasi durum 127"      $'ehco selam\n' 127
# Betik modunda oneri yalnizca bilgi; duzeltilmis komut KENDILIGINDEN
# kosmaz, cunku boyle bir kabuk kullanicinin yazmadigi seyi calistirir.
run_case   "oneri kendiliginden kosmaz"   $'ehco kosmamali\n' ""

# PATH'te okunabilen ama CALISTIRILAMAYAN dosyalar var; bunlari komut
# olarak onermek yanlis olur. Fikstur dizininde izinsiz bir dosya var ve
# yazilan sozcuk ona 1 uzaklikta; oneri cikmamali, 127 cikmali.
mkdir -p "$FIX/yol"
: > "$FIX/yol/zzqqxx"
chmod 644 "$FIX/yol/zzqqxx"
# PATH'i kabugun KENDISI daraltiyor, boylece aday kumesi tek dosyadan
# olusuyor. Ilk yazimda bu satir yoktu ve vaka bosa geciyordu: mutasyon
# denemesi dogrulamayi silmenin hicbir testi bozmadigini gosterdi.
run_stderr_absent "calistirilamayan aday adi gecmez" \
	"export PATH=$FIX/yol"$'\nzzqqxy\n' "zzqqxx"
run_status "calistirilamayan aday onerilmez" \
	"export PATH=$FIX/yol"$'\nzzqqxy\n' 127

# Yerlesik taramasini olcmek icin harici karsiligi OLMAYAN bir yerlesik
# gerekiyor: "ehco" icin PATH'te /bin/echo var, yani yerlesikler hic
# taranmasa da ayni oneri cikar. "export" boyle bir program degil.
# Yerlesikler taranmazsa en yakin aday 2 uzaklikta "expr" oluyor.
run_stderr "yerlesikler de aday olarak taranir" $'exprot A=1\n' \
	"bunu mu demek istediniz: export"

# env yerlesik DEGIL; PATH'teki program kullanilir. Tum ortami bastigi
# icin tam eslesme yerine satir arayan bir vaka gerekiyor.
run_case   "env PATH'ten kosar"           $'export NAXZ=bulundu\nenv | grep "^NAXZ="\n' "NAXZ=bulundu"

run_macro_clash_check
run_type_clash_check
run_norm_check
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
