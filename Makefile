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

CFLAGS      = $(CSTD) $(WARN) $(OPT) -Isrc

SRC         = $(wildcard src/*.c)
OBJ         = $(patsubst src/%.c,obj/%.o,$(SRC))
DEP         = $(OBJ:.o=.d)

ASAN_NAME   = $(NAME)-asan
ASAN_FLAGS  = -fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g3
ASAN_OBJ    = $(patsubst src/%.c,obj-asan/%.o,$(SRC))
ASAN_DEP    = $(ASAN_OBJ:.o=.d)

READLINE_H  = /usr/include/readline/readline.h

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

obj/%.o: src/%.c | obj
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
	@printf '  %-9s %s\n' 'cc' '$<'

obj:
	@mkdir -p obj

# Denetleyicili ikili: sizinti ve tanimsiz davranis bu ikilide yakalanir.
asan: readline-check $(ASAN_NAME)

$(ASAN_NAME): $(ASAN_OBJ)
	@$(CC) $(CSTD) $(WARN) $(ASAN_FLAGS) -Isrc $^ $(LDLIBS) -o $@
	@printf '  %-9s %s\n' 'link' '$@'

obj-asan/%.o: src/%.c | obj-asan
	@$(CC) $(CSTD) $(WARN) $(ASAN_FLAGS) -Isrc -MMD -MP -c $< -o $@
	@printf '  %-9s %s\n' 'cc asan' '$<'

obj-asan:
	@mkdir -p obj-asan

# Test betigini normal ikili ile kosar.
test: all
	@./test/run.sh

# Testleri once normal, sonra denetleyicili ikili ile kosar.
check: test asan
	@NAX_BIN=./$(ASAN_NAME) ./test/run.sh

clean:
	@rm -rf obj obj-asan $(NAME) $(ASAN_NAME)
	@printf '  %-9s %s\n' 'clean' 'obj obj-asan $(NAME) $(ASAN_NAME)'

-include $(DEP)
-include $(ASAN_DEP)
