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
SURF_MAN_SOURCES := \
	examples/surf-man/app/main.c \
	examples/surf-man/app/lifecycle.c \
	examples/surf-man/app/input.c \
	examples/surf-man/app/menu.c \
	examples/surf-man/app/engine_hooks.c \
	examples/surf-man/app/runtime.c \
	examples/surf-man/game/rules.c \
	examples/surf-man/game/wave.c \
	examples/surf-man/game/scoring.c \
	examples/surf-man/game/simulation.c \
	examples/surf-man/game/ride.c \
	examples/surf-man/presentation/common.c \
	examples/surf-man/presentation/effects.c \
	examples/surf-man/presentation/art.c \
	examples/surf-man/presentation/rider.c \
	examples/surf-man/presentation/hud.c \
	examples/surf-man/presentation/panels.c \
	examples/surf-man/presentation/render.c \
	examples/surf-man/qa/simulation_checks.c \
	examples/surf-man/qa/render_checks.c \
	examples/surf-man/qa/smoke.c
SURF_MAN_OBJECTS := $(patsubst examples/%.c,$(BUILD_DIR)/obj/examples/%.o,$(SURF_MAN_SOURCES))
SURF_MAN_DEPS := $(SURF_MAN_OBJECTS:.o=.d)
SCENE_TEST_SOURCES := tests/scene_reentrancy.c
CORE_TEST_SOURCES := tests/core_lifecycle.c
INPUT_TEST_SOURCES := tests/input_protocol.c
TERMINAL_TEST_SOURCES := tests/terminal_lifecycle.c
SAVE_TEST_SOURCES := tests/save_crc.c
ASSETS_TEST_SOURCES := tests/assets_contracts.c
CONFIG_BOUNDARIES_TEST_SOURCES := tests/config_parser_boundaries.c
SAVE_BOUNDARIES_TEST_SOURCES := tests/save_container_boundaries.c
CONFIG_TEST_SOURCES := tests/config_parser_scale.c tests/config_parser_scale_support.c
RENDERER_TEST_SOURCES := tests/renderer_diff.c tests/renderer_diff_cases.c tests/renderer_diff_model.c
ECS_TEST_SOURCES := tests/ecs_optimization.c tests/ecs_optimization_support.c
ECS_TEST_SOURCES += tests/ecs_lifecycle_cases.c tests/ecs_storage_cases.c
ASTAR_TEST_SOURCES := tests/astar_workspace.c tests/astar_workspace_support.c
WORLD_TEST_SOURCES := tests/world_algorithms.c tests/world_test_support.c
WORLD_TEST_SOURCES += tests/world_core_cases.c tests/world_visibility_cases.c
WORLD_TEST_SOURCES += tests/astar_correctness_cases.c
RENDERER_LIFECYCLE_TEST_SOURCES := tests/renderer_lifecycle.c
EFFECTS_TEST_SOURCES := tests/effects_regression.c
UI_TEST_SOURCES := tests/ui_regression.c
SCENE_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(SCENE_TEST_SOURCES))
CORE_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(CORE_TEST_SOURCES))
INPUT_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(INPUT_TEST_SOURCES))
TERMINAL_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(TERMINAL_TEST_SOURCES))
SAVE_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(SAVE_TEST_SOURCES))
ASSETS_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(ASSETS_TEST_SOURCES))
CONFIG_BOUNDARIES_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(CONFIG_BOUNDARIES_TEST_SOURCES))
SAVE_BOUNDARIES_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(SAVE_BOUNDARIES_TEST_SOURCES))
CONFIG_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(CONFIG_TEST_SOURCES))
RENDERER_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(RENDERER_TEST_SOURCES))
ECS_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(ECS_TEST_SOURCES))
ASTAR_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(ASTAR_TEST_SOURCES))
WORLD_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(WORLD_TEST_SOURCES))
RENDERER_LIFECYCLE_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(RENDERER_LIFECYCLE_TEST_SOURCES))
EFFECTS_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(EFFECTS_TEST_SOURCES))
UI_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(UI_TEST_SOURCES))
TEST_OBJECTS := $(SCENE_TEST_OBJECTS) $(SAVE_TEST_OBJECTS) $(CONFIG_TEST_OBJECTS)
TEST_OBJECTS += $(CORE_TEST_OBJECTS) $(INPUT_TEST_OBJECTS) $(TERMINAL_TEST_OBJECTS)
TEST_OBJECTS += $(ASSETS_TEST_OBJECTS) $(CONFIG_BOUNDARIES_TEST_OBJECTS) $(SAVE_BOUNDARIES_TEST_OBJECTS)
TEST_OBJECTS += $(RENDERER_TEST_OBJECTS) $(ECS_TEST_OBJECTS) $(ASTAR_TEST_OBJECTS)
TEST_OBJECTS += $(WORLD_TEST_OBJECTS)
TEST_OBJECTS += $(RENDERER_LIFECYCLE_TEST_OBJECTS) $(EFFECTS_TEST_OBJECTS) $(UI_TEST_OBJECTS)
CONFIG_TEST_SUBJECT_SOURCES := src/assets/config_parser.c src/assets/config_index.c src/assets/config_storage.c
CONFIG_TEST_SUBJECT_OBJECTS := $(patsubst src/assets/%.c,$(BUILD_DIR)/obj/tests/subjects/%.o,$(CONFIG_TEST_SUBJECT_SOURCES))
RENDERER_TEST_SUBJECT := $(BUILD_DIR)/obj/tests/subjects/renderer_ansi.o
TEST_DEPS := $(TEST_OBJECTS:.o=.d) $(CONFIG_TEST_SUBJECT_OBJECTS:.o=.d) $(RENDERER_TEST_SUBJECT:.o=.d)
COMMON_OBJECT := $(BUILD_DIR)/obj/core/common.o
ECS_SUBJECT_OBJECTS := $(BUILD_DIR)/obj/ecs/component.o $(BUILD_DIR)/obj/ecs/component_instances.o
ECS_SUBJECT_OBJECTS += $(BUILD_DIR)/obj/ecs/component_store.o $(BUILD_DIR)/obj/ecs/ecs.o
ECS_SUBJECT_OBJECTS += $(BUILD_DIR)/obj/ecs/entity.o $(BUILD_DIR)/obj/ecs/view.o
LIBRARY := $(BUILD_DIR)/lib/libaforc.a
ROGUELIKE := $(BUILD_DIR)/bin/aforc-roguelike
SURF_MAN := $(BUILD_DIR)/bin/aforc-surf-man
SCENE_TEST := $(BUILD_DIR)/bin/aforc-scene-reentrancy-test
CORE_TEST := $(BUILD_DIR)/bin/aforc-core-lifecycle-test
INPUT_TEST := $(BUILD_DIR)/bin/aforc-input-protocol-test
TERMINAL_TEST := $(BUILD_DIR)/bin/aforc-terminal-lifecycle-test
SAVE_TEST := $(BUILD_DIR)/bin/aforc-save-crc-test
ASSETS_TEST := $(BUILD_DIR)/bin/aforc-assets-contracts-test
CONFIG_BOUNDARIES_TEST := $(BUILD_DIR)/bin/aforc-config-parser-boundaries-test
SAVE_BOUNDARIES_TEST := $(BUILD_DIR)/bin/aforc-save-container-boundaries-test
CONFIG_TEST := $(BUILD_DIR)/bin/aforc-config-parser-scale-test
RENDERER_TEST := $(BUILD_DIR)/bin/aforc-renderer-diff-test
ECS_TEST := $(BUILD_DIR)/bin/aforc-ecs-optimization-test
ASTAR_TEST := $(BUILD_DIR)/bin/aforc-astar-workspace-test
WORLD_TEST := $(BUILD_DIR)/bin/aforc-world-algorithms-test
RENDERER_LIFECYCLE_TEST := $(BUILD_DIR)/bin/aforc-renderer-lifecycle-test
EFFECTS_TEST := $(BUILD_DIR)/bin/aforc-effects-regression-test
UI_TEST := $(BUILD_DIR)/bin/aforc-ui-regression-test
TEST_BINARIES := $(SCENE_TEST) $(SAVE_TEST) $(CONFIG_TEST)
TEST_BINARIES += $(CORE_TEST) $(INPUT_TEST) $(TERMINAL_TEST)
TEST_BINARIES += $(ASSETS_TEST) $(CONFIG_BOUNDARIES_TEST) $(SAVE_BOUNDARIES_TEST)
TEST_BINARIES += $(RENDERER_TEST) $(ECS_TEST) $(ASTAR_TEST)
TEST_BINARIES += $(WORLD_TEST)
TEST_BINARIES += $(RENDERER_LIFECYCLE_TEST) $(EFFECTS_TEST) $(UI_TEST)
FLAGS_STAMP := $(BUILD_DIR)/.build-flags

AFORC_CPPFLAGS := -Iinclude
AFORC_SURF_MAN_CPPFLAGS := -Iexamples/surf-man/include
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

.PHONY: all library example debug release strict sanitize smoke test run run-surf-man install clean help FORCE

FORCE:

$(FLAGS_STAMP): FORCE
	@mkdir -p "$(@D)"
	@signature='$(CC)|$(CPPFLAGS)|$(AFORC_CPPFLAGS)|$(AFORC_SURF_MAN_CPPFLAGS)|$(AFORC_LIBRARY_CPPFLAGS)|$(CFLAGS)|$(AFORC_CFLAGS)|$(AFORC_LIBRARY_CFLAGS)|$(AFORC_PROGRAM_CFLAGS)|$(LDFLAGS)|$(AFORC_LDFLAGS)|$(AFORC_PROGRAM_LDFLAGS)|$(LDLIBS)|$(AFORC_LDLIBS)'; \
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

example: $(ROGUELIKE) $(SURF_MAN)

$(LIBRARY): $(OBJECTS)
	@mkdir -p "$(@D)"
	$(AR) rcs "$@" $(OBJECTS)
	$(RANLIB) "$@"

$(BUILD_DIR)/obj/%.o: src/%.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(AFORC_LIBRARY_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_LIBRARY_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/obj/examples/%.o: examples/%.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(AFORC_EXAMPLE_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(SURF_MAN_OBJECTS): AFORC_EXAMPLE_CPPFLAGS := $(AFORC_SURF_MAN_CPPFLAGS)

$(BUILD_DIR)/obj/tests/%.o: tests/%.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(CONFIG_TEST_SUBJECT_OBJECTS): $(BUILD_DIR)/obj/tests/subjects/%.o: src/assets/%.c tests/config_parser_scale_support.h $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -include tests/config_parser_scale_support.h -Dmalloc=aforc_config_test_malloc -Drealloc=aforc_config_test_realloc -Dfree=aforc_config_test_free -MMD -MP -c "$<" -o "$@"

$(RENDERER_TEST_SUBJECT): src/render/renderer_ansi.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(ROGUELIKE): $(ROGUELIKE_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(ROGUELIKE_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(SURF_MAN): $(SURF_MAN_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(SURF_MAN_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(SCENE_TEST): $(SCENE_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(SCENE_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(CORE_TEST): $(CORE_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(CORE_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(INPUT_TEST): $(INPUT_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(INPUT_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(TERMINAL_TEST): $(TERMINAL_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(TERMINAL_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(SAVE_TEST): $(SAVE_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(SAVE_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(ASSETS_TEST): $(ASSETS_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(ASSETS_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(CONFIG_BOUNDARIES_TEST): $(CONFIG_BOUNDARIES_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(CONFIG_BOUNDARIES_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(SAVE_BOUNDARIES_TEST): $(SAVE_BOUNDARIES_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(SAVE_BOUNDARIES_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(CONFIG_TEST): $(CONFIG_TEST_OBJECTS) $(CONFIG_TEST_SUBJECT_OBJECTS) $(COMMON_OBJECT)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(CONFIG_TEST_OBJECTS) $(CONFIG_TEST_SUBJECT_OBJECTS) $(COMMON_OBJECT) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(RENDERER_TEST): $(RENDERER_TEST_OBJECTS) $(RENDERER_TEST_SUBJECT) $(COMMON_OBJECT) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(RENDERER_TEST_OBJECTS) $(RENDERER_TEST_SUBJECT) $(COMMON_OBJECT) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(ECS_TEST): $(ECS_TEST_OBJECTS) $(COMMON_OBJECT) $(ECS_SUBJECT_OBJECTS)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(ECS_TEST_OBJECTS) $(COMMON_OBJECT) $(ECS_SUBJECT_OBJECTS) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(ASTAR_TEST): $(ASTAR_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(ASTAR_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(WORLD_TEST): $(WORLD_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(WORLD_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(RENDERER_LIFECYCLE_TEST): $(RENDERER_LIFECYCLE_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(RENDERER_LIFECYCLE_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(EFFECTS_TEST): $(EFFECTS_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(EFFECTS_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(UI_TEST): $(UI_TEST_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(UI_TEST_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

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

smoke: $(ROGUELIKE) $(SURF_MAN)
	"$(ROGUELIKE)" --smoke
	"$(SURF_MAN)" --smoke

test: smoke $(TEST_BINARIES)
	"$(SURF_MAN)" --help
	! "$(SURF_MAN)" --seed nope
	! "$(SURF_MAN)" --seed -1 --smoke
	"$(SCENE_TEST)"
	"$(CORE_TEST)"
	"$(INPUT_TEST)"
	"$(TERMINAL_TEST)"
	"$(SAVE_TEST)"
	"$(ASSETS_TEST)"
	"$(CONFIG_BOUNDARIES_TEST)"
	"$(SAVE_BOUNDARIES_TEST)"
	"$(CONFIG_TEST)"
	"$(RENDERER_TEST)"
	"$(ECS_TEST)"
	"$(ASTAR_TEST)"
	"$(WORLD_TEST)"
	"$(RENDERER_LIFECYCLE_TEST)"
	"$(EFFECTS_TEST)"
	"$(UI_TEST)"

run: $(ROGUELIKE)
	"$(ROGUELIKE)"

run-surf-man: $(SURF_MAN)
	"$(SURF_MAN)"

install: $(LIBRARY)
	install -d "$(DESTDIR)$(PREFIX)/lib" "$(DESTDIR)$(PREFIX)/include"
	install -m 0644 "$(LIBRARY)" "$(DESTDIR)$(PREFIX)/lib/libaforc.a"
	cp -R include/aforc "$(DESTDIR)$(PREFIX)/include/"

clean:
	rm -rf "$(BUILD_DIR)"

help:
	@printf '%s\n' \
		'all       Build the static library and example programs (default)' \
		'example   Build the roguelike and Surf-Man examples' \
		'debug     Build unoptimized binaries with debug symbols' \
		'release   Build optimized binaries' \
		'strict    Rebuild with warnings promoted to errors, then test' \
		'sanitize  Rebuild with ASan/UBSan, then test' \
		'smoke     Run deterministic, non-interactive example smokes' \
		'test      Run example CLI checks, smoke, and regression tests' \
		'run       Launch the playable terminal roguelike' \
		'run-surf-man Launch the playable terminal Surf-Man example' \
		'install   Install the static library and public headers' \
		'HARDEN=0  Disable supported build hardening (default: 1)' \
		'clean     Remove Make build artifacts'

-include $(DEPS) $(ROGUELIKE_DEPS) $(SURF_MAN_DEPS) $(TEST_DEPS)
