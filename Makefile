BUILD_DIR = build

# libcommon config
export COMMON_OUT_DIR=../$(BUILD_DIR)

METEOR_SRC_PATHS = \
	src/*.c \
	src/microui/*.c \

METEOR_SRC = $(wildcard $(METEOR_SRC_PATHS))
METEOR_OBJECTS = $(METEOR_SRC:src/%.c=build/%.o)

CC ?= gcc
LD = $(CC)

INCLUDEPATHS = -Iinclude/ -Icommon/include/
ASANFLAGS = -fsanitize=undefined -fsanitize=address
CFLAGS = -std=gnu2x -fwrapv -fno-strict-aliasing
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
all: meteor

bin/libcommon.a:
	$(MAKE) -C common
	cp $(BUILD_DIR)/libcommon.a bin/libcommon.a

build/%.o: src/%.c
	$(shell echo 1>&2 -e "Compiling $<")
	@$(CC) -c -o $@ $< -MD $(INCLUDEPATHS) $(ALLFLAGS) $(OPT)

.PHONY: meteor
meteor: bin/meteor
bin/meteor:  $(METEOR_OBJECTS) bin/libcommon.a
	@$(LD) $(LDFLAGS) $(METEOR_OBJECTS) -o bin/meteor -lm -lc -Lbin -lcommon -lSDL3 -lSDL3_ttf

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)/
	@rm -rf bin/
	@mkdir $(BUILD_DIR)/
	@mkdir bin/
	@mkdir -p $(dir $(METEOR_OBJECTS))

# generate compile commands with bear if u got it!!!
# very good highly recommended ʕ·ᴥ·ʔ
.PHONY: bear-gen-cc
bear-gen-cc: clean
	bear -- $(MAKE) all

-include $(METEOR_OBJECTS:.o=.d)
