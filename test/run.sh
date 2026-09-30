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

run_stderr "<< henuz kosmuyor"            $'cat << SON\n'         "<< yonlendirmesi bu asamada calistirilmiyor"
run_status "<< kodu 1"                    $'cat << SON\n'         1

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
