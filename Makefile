# nax kabugu — derleme kurallari.
#
# NEDEN BOYLE:
#   pkg-config bu ortamda yok, o yuzden readline elle linklenir ve baslik
#   dosyasinin varligi derleme oncesinde kontrol edilir. libreadline.so.8
#   zaten libtinfo'ya bagli oldugu icin -ltinfo EKLENMEZ.
#   valgrind de yok; bellek dogrulamasi "make asan" hedefinden gelir.
#
# Hedefler:
#   all     kabugu derler (varsayilan)
#   asan    adres + tanimsiz davranis denetleyicili ikinci bir ikili uretir
#   test    test betigini normal ikili ile kosar
#   check   testleri hem normal hem denetleyicili ikili ile kosar
#   clean   uretilen her seyi siler
#
# Kullanim:
#   make            uyarilar hata sayilir
#   make W=0        -Werror kapali (hizli deneme icin)

NAME        = nax
CC          = cc
CSTD        = -std=c99 -D_GNU_SOURCE
WARN        = -Wall -Wextra -Wshadow -Wvla -Wstrict-prototypes -Wformat=2
OPT         = -O2 -g
LDLIBS      = -lreadline

W          ?= 1
ifeq ($(W),1)
WARN       += -Werror
endif

# NEDEN BIRDEN FAZLA -I:
#   Kaynaklar src/ altinda alan alan klasorlenmis (core, parse, exec). Her
#   alanin basligi kendi klasorunde duruyor; bu -I listesi sayesinde
#   dosyalarda "parse.h" yazmak yeterli, yola gerek kalmiyor. Gruplama
#   okunabilirlik icin, dahil etme satirlarini uzatmak icin degil.
INC         = -Isrc -Isrc/parse -Isrc/exec
CFLAGS      = $(CSTD) $(WARN) $(OPT) $(INC)

# Alt klasorler de taranir; nesne agaci kaynak agacini AYNALAR, boylece
# ayni adda iki kaynak dosya olsa bile nesneleri carpismaz.
SRC         = $(wildcard src/*.c src/*/*.c)
OBJ         = $(patsubst src/%.c,obj/%.o,$(SRC))
DEP         = $(OBJ:.o=.d)

ASAN_NAME   = $(NAME)-asan
ASAN_FLAGS  = -fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g3
ASAN_OBJ    = $(patsubst src/%.c,obj-asan/%.o,$(SRC))
ASAN_DEP    = $(ASAN_OBJ:.o=.d)

READLINE_H  = /usr/include/readline/readline.h

# Birim testleri kabuk ikilisini hic baslatmaz, dogrudan modulleri cagirir.
# Bu yuzden main.c disindaki nesneler ayri bir kume olarak tutulur.
LIB_SRC     = $(filter-out src/main.c,$(SRC))
LIB_OBJ     = $(patsubst src/%.c,obj/%.o,$(LIB_SRC))
LIB_ASAN    = $(patsubst src/%.c,obj-asan/%.o,$(LIB_SRC))

UNIT_LIB    = test/harness.c
UNIT_SRC    = $(wildcard test/test_*.c)
UNIT_BIN    = $(patsubst test/test_%.c,test/bin/test_%,$(UNIT_SRC))
UNIT_BASAN  = $(patsubst test/test_%.c,test/bin/test_%.asan,$(UNIT_SRC))

.PHONY: all asan test check clean readline-check

# Varsayilan hedef: kabugun normal ikilisini uretir.
all: readline-check $(NAME)

# readline gelistirme basliklari yoksa anlasilir bir mesajla durur.
readline-check:
	@test -f $(READLINE_H) || { \
		printf 'HATA: readline basliklari bulunamadi (%s)\n' '$(READLINE_H)'; \
		printf '  cozum: sudo apt-get install -y libreadline-dev\n'; \
		exit 1; }

$(NAME): $(OBJ)
	@$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@
	@printf '  %-9s %s\n' 'link' '$@'

obj/%.o: src/%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
	@printf '  %-9s %s\n' 'cc' '$<' 

# Denetleyicili ikili: sizinti ve tanimsiz davranis bu ikilide yakalanir.
asan: readline-check $(ASAN_NAME)

$(ASAN_NAME): $(ASAN_OBJ)
	@$(CC) $(CSTD) $(WARN) $(ASAN_FLAGS) -Isrc $^ $(LDLIBS) -o $@
	@printf '  %-9s %s\n' 'link' '$@'

obj-asan/%.o: src/%.c
	@mkdir -p $(@D)
	@$(CC) $(CSTD) $(WARN) $(ASAN_FLAGS) $(INC) -MMD -MP -c $< -o $@
	@printf '  %-9s %s\n' 'cc asan' '$<' 

test/bin:
	@mkdir -p test/bin

test/bin/test_%: test/test_%.c $(UNIT_LIB) $(LIB_OBJ) | test/bin
	@$(CC) $(CFLAGS) $< $(UNIT_LIB) $(LIB_OBJ) $(LDLIBS) -o $@
	@printf '  %-9s %s\n' 'link' '$@'

test/bin/test_%.asan: test/test_%.c $(UNIT_LIB) $(LIB_ASAN) | test/bin
	@$(CC) $(CSTD) $(WARN) $(ASAN_FLAGS) $(INC) $< $(UNIT_LIB) $(LIB_ASAN) $(LDLIBS) -o $@
	@printf '  %-9s %s\n' 'link' '$@'

# Testleri normal ikili ile kosar.
test: all $(UNIT_BIN)
	@NAX_UNITS="$(UNIT_BIN)" ./test/run.sh

# Testleri once normal, sonra denetleyicili ikili ile kosar.
check: test asan $(UNIT_BASAN)
	@NAX_BIN=./$(ASAN_NAME) NAX_UNITS="$(UNIT_BASAN)" ./test/run.sh

clean:
	@rm -rf obj obj-asan test/bin test/__pycache__ $(NAME) $(ASAN_NAME)
	@printf '  %-9s %s\n' 'clean' 'obj obj-asan test/bin $(NAME) $(ASAN_NAME)'

-include $(DEP)
-include $(ASAN_DEP)
