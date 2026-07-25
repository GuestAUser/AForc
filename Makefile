# AForc
# Author: GuestAUser
# SPDX-License-Identifier: MIT

CC ?= cc
AR ?= ar
RANLIB ?= ranlib

PREFIX ?= /usr/local
BUILD_DIR ?= build/make
MODE ?= release
STRICT ?= 0
SANITIZE ?= 0
HARDEN ?= 1

SOURCES := $(sort $(wildcard src/*/*.c))
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/obj/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
ROGUELIKE_SOURCES := $(sort $(wildcard examples/roguelike/*.c))
ROGUELIKE_OBJECTS := $(patsubst examples/%.c,$(BUILD_DIR)/obj/examples/%.o,$(ROGUELIKE_SOURCES))
ROGUELIKE_DEPS := $(ROGUELIKE_OBJECTS:.o=.d)
SCENE_TEST_SOURCES := tests/scene_reentrancy.c
SAVE_TEST_SOURCES := tests/save_crc.c
CONFIG_TEST_SOURCES := tests/config_parser_scale.c tests/config_parser_scale_support.c
RENDERER_TEST_SOURCES := tests/renderer_diff.c tests/renderer_diff_cases.c tests/renderer_diff_model.c
ECS_TEST_SOURCES := tests/ecs_optimization.c tests/ecs_optimization_support.c
ASTAR_TEST_SOURCES := tests/astar_workspace.c tests/astar_workspace_support.c
SCENE_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(SCENE_TEST_SOURCES))
SAVE_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(SAVE_TEST_SOURCES))
CONFIG_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(CONFIG_TEST_SOURCES))
RENDERER_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(RENDERER_TEST_SOURCES))
ECS_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(ECS_TEST_SOURCES))
ASTAR_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(ASTAR_TEST_SOURCES))
TEST_OBJECTS := $(SCENE_TEST_OBJECTS) $(SAVE_TEST_OBJECTS) $(CONFIG_TEST_OBJECTS)
TEST_OBJECTS += $(RENDERER_TEST_OBJECTS) $(ECS_TEST_OBJECTS) $(ASTAR_TEST_OBJECTS)
CONFIG_TEST_SUBJECT := $(BUILD_DIR)/obj/tests/subjects/config_parser.o
RENDERER_TEST_SUBJECT := $(BUILD_DIR)/obj/tests/subjects/renderer_ansi.o
TEST_DEPS := $(TEST_OBJECTS:.o=.d) $(CONFIG_TEST_SUBJECT:.o=.d) $(RENDERER_TEST_SUBJECT:.o=.d)
COMMON_OBJECT := $(BUILD_DIR)/obj/core/common.o
ECS_SUBJECT_OBJECTS := $(BUILD_DIR)/obj/ecs/component.o
ECS_SUBJECT_OBJECTS += $(BUILD_DIR)/obj/ecs/ecs.o $(BUILD_DIR)/obj/ecs/view.o
LIBRARY := $(BUILD_DIR)/lib/libaforc.a
ROGUELIKE := $(BUILD_DIR)/bin/aforc-roguelike
SCENE_TEST := $(BUILD_DIR)/bin/aforc-scene-reentrancy-test
SAVE_TEST := $(BUILD_DIR)/bin/aforc-save-crc-test
CONFIG_TEST := $(BUILD_DIR)/bin/aforc-config-parser-scale-test
RENDERER_TEST := $(BUILD_DIR)/bin/aforc-renderer-diff-test
ECS_TEST := $(BUILD_DIR)/bin/aforc-ecs-optimization-test
ASTAR_TEST := $(BUILD_DIR)/bin/aforc-astar-workspace-test
TEST_BINARIES := $(SCENE_TEST) $(SAVE_TEST) $(CONFIG_TEST)
TEST_BINARIES += $(RENDERER_TEST) $(ECS_TEST) $(ASTAR_TEST)
FLAGS_STAMP := $(BUILD_DIR)/.build-flags

AFORC_CPPFLAGS := -Iinclude
AFORC_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic
AFORC_CFLAGS += -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes
AFORC_LDLIBS := -lm -pthread
AFORC_LIBRARY_CPPFLAGS :=
AFORC_LIBRARY_CFLAGS :=
AFORC_PROGRAM_CFLAGS :=
AFORC_PROGRAM_LDFLAGS :=

ifeq ($(MODE),debug)
AFORC_CFLAGS += -O0 -g3
else ifeq ($(MODE),release)
AFORC_CFLAGS += -O2 -DNDEBUG
else
$(error MODE must be either debug or release)
endif

ifeq ($(STRICT),1)
AFORC_CFLAGS += -Werror
endif

ifeq ($(SANITIZE),1)
AFORC_CFLAGS += -fno-omit-frame-pointer -fsanitize=address,undefined
AFORC_LDFLAGS += -fsanitize=address,undefined
endif

ifeq ($(HARDEN),1)
ifneq ($(SANITIZE),1)
AFORC_HARDENING_SUPPORTED := $(shell \
	printf '%s\n' \
		'_Static_assert(__USE_FORTIFY_LEVEL >= 3, "fortify level 3 required");' \
		'int main(void) { return 0; }' | \
	$(CC) -include features.h -x c - -std=c17 -O2 -Werror \
		-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
		-fstack-protector-strong -fvisibility=hidden -fPIE -pie \
		-Wl,-z,relro -Wl,-z,now -o /dev/null >/dev/null 2>&1 && \
	printf '1')
ifeq ($(AFORC_HARDENING_SUPPORTED),1)
AFORC_LIBRARY_CPPFLAGS += -DAFORC_SHARED -DAFORC_BUILDING_LIBRARY
AFORC_LIBRARY_CFLAGS += -fstack-protector-strong -fvisibility=hidden -fPIC
AFORC_PROGRAM_CFLAGS += -fstack-protector-strong -fPIE
AFORC_PROGRAM_LDFLAGS += -pie -Wl,-z,relro -Wl,-z,now
ifeq ($(MODE),release)
AFORC_LIBRARY_CFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
AFORC_PROGRAM_CFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
endif
else
$(warning HARDEN=1 requested, but the complete hardening set is unsupported; building without it)
endif
endif
else ifneq ($(HARDEN),0)
$(error HARDEN must be either 0 or 1)
endif

.DEFAULT_GOAL := all

.PHONY: all library example debug release strict sanitize smoke test run install clean help FORCE

FORCE:

$(FLAGS_STAMP): FORCE
	@mkdir -p "$(@D)"
	@signature='$(CC)|$(CPPFLAGS)|$(AFORC_CPPFLAGS)|$(AFORC_LIBRARY_CPPFLAGS)|$(CFLAGS)|$(AFORC_CFLAGS)|$(AFORC_LIBRARY_CFLAGS)|$(AFORC_PROGRAM_CFLAGS)|$(LDFLAGS)|$(AFORC_LDFLAGS)|$(AFORC_PROGRAM_LDFLAGS)|$(LDLIBS)|$(AFORC_LDLIBS)'; \
	temporary="$@.tmp"; \
	printf '%s\n' "$$signature" > "$$temporary"; \
	if ! cmp -s "$$temporary" "$@"; then \
		rm -rf "$(BUILD_DIR)/obj" "$(BUILD_DIR)/lib" "$(BUILD_DIR)/bin"; \
		mv "$$temporary" "$@"; \
	else \
		rm -f "$$temporary"; \
	fi

all: library example

library: $(LIBRARY)

example: $(ROGUELIKE)

$(LIBRARY): $(OBJECTS)
	@mkdir -p "$(@D)"
	$(AR) rcs "$@" $(OBJECTS)
	$(RANLIB) "$@"

$(BUILD_DIR)/obj/%.o: src/%.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(AFORC_LIBRARY_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_LIBRARY_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/obj/examples/%.o: examples/%.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/obj/tests/%.o: tests/%.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(CONFIG_TEST_SUBJECT): src/assets/config_parser.c tests/config_parser_scale_support.h $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -include tests/config_parser_scale_support.h -Dmalloc=aforc_config_test_malloc -Drealloc=aforc_config_test_realloc -Dfree=aforc_config_test_free -MMD -MP -c "$<" -o "$@"

$(RENDERER_TEST_SUBJECT): src/render/renderer_ansi.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(ROGUELIKE): $(ROGUELIKE_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(ROGUELIKE_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(SCENE_TEST): $(SCENE_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(SCENE_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(SAVE_TEST): $(SAVE_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(SAVE_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(CONFIG_TEST): $(CONFIG_TEST_OBJECTS) $(CONFIG_TEST_SUBJECT) $(COMMON_OBJECT)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(CONFIG_TEST_OBJECTS) $(CONFIG_TEST_SUBJECT) $(COMMON_OBJECT) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(RENDERER_TEST): $(RENDERER_TEST_OBJECTS) $(RENDERER_TEST_SUBJECT) $(COMMON_OBJECT) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(RENDERER_TEST_OBJECTS) $(RENDERER_TEST_SUBJECT) $(COMMON_OBJECT) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(ECS_TEST): $(ECS_TEST_OBJECTS) $(COMMON_OBJECT) $(ECS_SUBJECT_OBJECTS)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(ECS_TEST_OBJECTS) $(COMMON_OBJECT) $(ECS_SUBJECT_OBJECTS) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(ASTAR_TEST): $(ASTAR_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(ASTAR_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

debug:
	$(MAKE) clean
	$(MAKE) MODE=debug all

release:
	$(MAKE) clean
	$(MAKE) MODE=release all

strict:
	$(MAKE) clean
	$(MAKE) MODE=debug STRICT=1 all test

sanitize:
	$(MAKE) clean
	$(MAKE) MODE=debug STRICT=1 SANITIZE=1 all test

smoke: $(ROGUELIKE)
	"$(ROGUELIKE)" --smoke

test: smoke $(TEST_BINARIES)
	"$(SCENE_TEST)"
	"$(SAVE_TEST)"
	"$(CONFIG_TEST)"
	"$(RENDERER_TEST)"
	"$(ECS_TEST)"
	"$(ASTAR_TEST)"

run: $(ROGUELIKE)
	"$(ROGUELIKE)"

install: $(LIBRARY)
	install -d "$(DESTDIR)$(PREFIX)/lib" "$(DESTDIR)$(PREFIX)/include"
	install -m 0644 "$(LIBRARY)" "$(DESTDIR)$(PREFIX)/lib/libaforc.a"
	cp -R include/aforc "$(DESTDIR)$(PREFIX)/include/"

clean:
	rm -rf "$(BUILD_DIR)"

help:
	@printf '%s\n' \
		'all       Build the static library and roguelike (default)' \
		'debug     Build unoptimized binaries with debug symbols' \
		'release   Build optimized binaries' \
		'strict    Rebuild with warnings promoted to errors, then test' \
		'sanitize  Rebuild with ASan/UBSan, then test' \
		'smoke     Run the deterministic, non-interactive integration smoke' \
		'test      Run smoke and all regression tests' \
		'run       Launch the playable terminal roguelike' \
		'install   Install the static library and public headers' \
		'HARDEN=0  Disable supported build hardening (default: 1)' \
		'clean     Remove Make build artifacts'

-include $(DEPS) $(ROGUELIKE_DEPS) $(TEST_DEPS)
