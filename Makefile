BUILD_DIR = build

# libcommon config
export COMMON_OUT_DIR=../$(BUILD_DIR)

TACHYON_SRC_PATHS = \
	src/*.c \
	src/targets/*.c \
	src/asm/*.c \
	src/targets/chasm/*.c \
	src/tomlc17/*.c \
	

TACHYON_SRC = $(wildcard $(TACHYON_SRC_PATHS))
TACHYON_OBJECTS = $(TACHYON_SRC:src/%.c=build/%.o)

CC ?= gcc
LD = $(CC)

INCLUDEPATHS = -Iinclude/ -Icommon/include/
ASANFLAGS = -fsanitize=undefined -fsanitize=address
CFLAGS = -std=c23 -fwrapv -fno-strict-aliasing -masm=intel
WARNINGS = \
	-Wall -Wimplicit-fallthrough -Wmaybe-uninitialized \
	-Wno-enum-compare -Wno-unused -Wno-enum-conversion -Wno-discarded-qualifiers
ALLFLAGS = $(CFLAGS) $(WARNINGS)
OPT = -g3 -O0

LDFLAGS =

ifneq ($(OS),Windows_NT)
	CFLAGS += -rdynamic
endif

ifdef ASAN_ENABLE
	CFLAGS += $(ASANFLAGS)
	LDFLAGS += $(ASANFLAGS)
endif

#include configuration file, if present
-include config.mk

.PHONY: all
all: tachyon

bin/libcommon.a:
	$(MAKE) -C common
	cp $(BUILD_DIR)/libcommon.a bin/libcommon.a

build/%.o: src/%.c
	$(shell echo 1>&2 -e "Compiling $<")
	@$(CC) -c -o $@ $< -MD $(INCLUDEPATHS) $(ALLFLAGS) $(OPT)

.PHONY: tachyon
tachyon: bin/tachyon
bin/tachyon:  $(TACHYON_OBJECTS) bin/libcommon.a
	@$(LD) $(LDFLAGS) $(TACHYON_OBJECTS) -o bin/tachyon -lm -lc -Lbin -lcommon -lSDL3

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)/
	@rm -rf bin/
	@mkdir $(BUILD_DIR)/
	@mkdir bin/
	@mkdir -p $(dir $(TACHYON_OBJECTS))

# generate compile commands with bear if u got it!!!
# very good highly recommended ʕ·ᴥ·ʔ
.PHONY: bear-gen-cc
bear-gen-cc: clean
	bear -- $(MAKE) all

-include $(TACHYON_OBJECTS:.o=.d)
