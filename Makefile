# AForc
# Author: GuestAUser
# SPDX-License-Identifier: MIT

CC ?= cc
AR ?= ar
RANLIB ?= ranlib
PKG_CONFIG ?= pkg-config

PREFIX ?= /usr/local
INSTALL_LIBDIR ?= lib
INSTALL_INCLUDEDIR ?= include
INSTALL_DATADIR ?= share
BUILD_DIR ?= build/make
MODE ?= release
STRICT ?= 0
SANITIZE ?= 0
HARDEN ?= 1
AFORC_VERSION := 0.1.0

SOURCES := $(sort $(wildcard src/*/*.c))
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/obj/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
ROGUELIKE_SOURCES := \
	examples/roguelike/main.c \
	examples/roguelike/app/engine_hooks.c \
	examples/roguelike/app/input_actions.c \
	examples/roguelike/app/runtime.c \
	examples/roguelike/game/enemy_ai.c \
	examples/roguelike/game/floor_population.c \
	examples/roguelike/game/game.c \
	examples/roguelike/game/game_entities.c \
	examples/roguelike/game/game_rules.c \
	examples/roguelike/game/generation.c \
	examples/roguelike/game/player_actions.c \
	examples/roguelike/persistence/persistence.c \
	examples/roguelike/presentation/effects.c \
	examples/roguelike/presentation/render.c \
	examples/roguelike/presentation/render_ui.c \
	examples/roguelike/presentation/render_world.c \
	examples/roguelike/qa/runtime_checks.c \
	examples/roguelike/qa/smoke.c
ROGUELIKE_OBJECTS := $(patsubst examples/%.c,$(BUILD_DIR)/obj/examples/%.o,$(ROGUELIKE_SOURCES))
ROGUELIKE_DEPS := $(ROGUELIKE_OBJECTS:.o=.d)
FIELDZERO_DOMAIN_SOURCES := \
	examples/fieldzero/content/horizon.c \
	examples/fieldzero/content/origin.c \
	examples/fieldzero/content/registry.c \
	examples/fieldzero/content/shear.c \
	examples/fieldzero/content/span.c \
	examples/fieldzero/content/well.c \
	examples/fieldzero/game/collision.c \
	examples/fieldzero/game/game.c \
	examples/fieldzero/game/progression.c \
	examples/fieldzero/game/simulation.c \
	examples/fieldzero/presentation/effects.c \
	examples/fieldzero/presentation/render.c \
	examples/fieldzero/presentation/render_ui.c \
	examples/fieldzero/presentation/render_world.c \
	examples/fieldzero/qa/digests.c \
	examples/fieldzero/qa/regression.c \
	examples/fieldzero/qa/validators.c
FIELDZERO_SOURCES := \
	examples/fieldzero/main.c \
	examples/fieldzero/app/engine_hooks.c \
	examples/fieldzero/app/input_actions.c \
	examples/fieldzero/app/runtime.c \
	examples/fieldzero/app/scenes.c \
	examples/fieldzero/qa/smoke.c \
	$(FIELDZERO_DOMAIN_SOURCES)
FIELDZERO_DOMAIN_OBJECTS := $(patsubst examples/%.c,$(BUILD_DIR)/obj/examples/%.o,$(FIELDZERO_DOMAIN_SOURCES))
FIELDZERO_OBJECTS := $(patsubst examples/%.c,$(BUILD_DIR)/obj/examples/%.o,$(FIELDZERO_SOURCES))
FIELDZERO_REGRESSION_OBJECTS := $(BUILD_DIR)/obj/examples/fieldzero/qa/regression_main.o $(FIELDZERO_DOMAIN_OBJECTS)
FIELDZERO_DEPS := $(FIELDZERO_OBJECTS:.o=.d) $(FIELDZERO_REGRESSION_OBJECTS:.o=.d)
SCENE_TEST_SOURCES := tests/scene_reentrancy.c
CORE_TEST_SOURCES := tests/core_lifecycle.c
INPUT_TEST_SOURCES := tests/input_protocol.c
TERMINAL_TEST_SOURCES := tests/terminal_lifecycle.c
FIELDZERO_PTY_TEST_SOURCES := tests/fieldzero_pty.c
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
FIELDZERO_PTY_TEST_OBJECTS := $(patsubst tests/%.c,$(BUILD_DIR)/obj/tests/%.o,$(FIELDZERO_PTY_TEST_SOURCES))
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
TEST_OBJECTS += $(FIELDZERO_PTY_TEST_OBJECTS)
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
PKG_CONFIG_FILE := $(BUILD_DIR)/aforc.pc
PACKAGE_TEST_ROOT ?= $(BUILD_DIR)/package-test
PACKAGE_TEST_STAGE := $(abspath $(PACKAGE_TEST_ROOT)/stage)
PACKAGE_TEST_CONSUMER_BUILD := $(abspath $(PACKAGE_TEST_ROOT)/consumer)
ROGUELIKE := $(BUILD_DIR)/bin/aforc-roguelike
FIELDZERO := $(BUILD_DIR)/bin/aforc-fieldzero
FIELDZERO_REGRESSION := $(BUILD_DIR)/bin/aforc-fieldzero-regression
SCENE_TEST := $(BUILD_DIR)/bin/aforc-scene-reentrancy-test
CORE_TEST := $(BUILD_DIR)/bin/aforc-core-lifecycle-test
INPUT_TEST := $(BUILD_DIR)/bin/aforc-input-protocol-test
TERMINAL_TEST := $(BUILD_DIR)/bin/aforc-terminal-lifecycle-test
FIELDZERO_PTY_TEST := $(BUILD_DIR)/bin/aforc-fieldzero-pty-test
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
TEST_BINARIES += $(FIELDZERO_PTY_TEST)
TEST_BINARIES += $(ASSETS_TEST) $(CONFIG_BOUNDARIES_TEST) $(SAVE_BOUNDARIES_TEST)
TEST_BINARIES += $(RENDERER_TEST) $(ECS_TEST) $(ASTAR_TEST)
TEST_BINARIES += $(WORLD_TEST)
TEST_BINARIES += $(RENDERER_LIFECYCLE_TEST) $(EFFECTS_TEST) $(UI_TEST)
TEST_BINARIES += $(FIELDZERO_REGRESSION)
FLAGS_STAMP := $(BUILD_DIR)/.build-flags

AFORC_CPPFLAGS := -Iinclude
AFORC_ROGUELIKE_CPPFLAGS := -Iexamples/roguelike/include
AFORC_FIELDZERO_CPPFLAGS := -Iexamples/fieldzero/include
AFORC_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic
AFORC_CFLAGS += -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes
AFORC_LDLIBS :=
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

.PHONY: all library example debug release strict sanitize smoke test package-test run run-fieldzero install clean help FORCE

FORCE:

$(FLAGS_STAMP): FORCE
	@mkdir -p "$(@D)"
	@signature='$(CC)|$(CPPFLAGS)|$(AFORC_CPPFLAGS)|$(AFORC_ROGUELIKE_CPPFLAGS)|$(AFORC_FIELDZERO_CPPFLAGS)|$(AFORC_LIBRARY_CPPFLAGS)|$(CFLAGS)|$(AFORC_CFLAGS)|$(AFORC_LIBRARY_CFLAGS)|$(AFORC_PROGRAM_CFLAGS)|$(LDFLAGS)|$(AFORC_LDFLAGS)|$(AFORC_PROGRAM_LDFLAGS)|$(LDLIBS)|$(AFORC_LDLIBS)'; \
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

example: $(ROGUELIKE) $(FIELDZERO)

$(LIBRARY): $(OBJECTS)
	@mkdir -p "$(@D)"
	$(AR) rcs "$@" $(OBJECTS)
	$(RANLIB) "$@"

$(PKG_CONFIG_FILE): cmake/aforc.pc.in Makefile
	@mkdir -p "$(@D)"
	@pc_prefix_relative="$$(printf '%s\n' "$(INSTALL_LIBDIR)" | \
		awk -F/ '{ relative=".."; for (part=1; part<=NF; ++part) relative="../" relative; print relative }')"; \
	sed \
		-e 's|@PROJECT_VERSION@|$(AFORC_VERSION)|g' \
		-e 's|@CMAKE_INSTALL_LIBDIR@|$(INSTALL_LIBDIR)|g' \
		-e 's|@CMAKE_INSTALL_INCLUDEDIR@|$(INSTALL_INCLUDEDIR)|g' \
		-e "s|@AFORC_PC_PREFIX_RELATIVE@|$$pc_prefix_relative|g" \
		"$<" > "$@"

$(BUILD_DIR)/obj/%.o: src/%.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(AFORC_LIBRARY_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_LIBRARY_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/obj/examples/%.o: examples/%.c $(FLAGS_STAMP)
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(AFORC_CPPFLAGS) $(AFORC_EXAMPLE_CPPFLAGS) $(CFLAGS) $(AFORC_CFLAGS) $(AFORC_PROGRAM_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(ROGUELIKE_OBJECTS): AFORC_EXAMPLE_CPPFLAGS := $(AFORC_ROGUELIKE_CPPFLAGS)
$(FIELDZERO_OBJECTS) $(FIELDZERO_REGRESSION_OBJECTS): AFORC_EXAMPLE_CPPFLAGS := $(AFORC_FIELDZERO_CPPFLAGS)

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

$(FIELDZERO): $(FIELDZERO_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(FIELDZERO_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

$(FIELDZERO_REGRESSION): $(FIELDZERO_REGRESSION_OBJECTS) $(LIBRARY)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(FIELDZERO_REGRESSION_OBJECTS) $(LIBRARY) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

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

$(FIELDZERO_PTY_TEST): $(FIELDZERO_PTY_TEST_OBJECTS) $(FIELDZERO)
	@mkdir -p "$(@D)"
	$(CC) $(LDFLAGS) $(AFORC_LDFLAGS) $(AFORC_PROGRAM_LDFLAGS) $(FIELDZERO_PTY_TEST_OBJECTS) $(LDLIBS) $(AFORC_LDLIBS) -o "$@"

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

smoke: $(ROGUELIKE) $(FIELDZERO)
	"$(ROGUELIKE)" --smoke
	"$(FIELDZERO)" --smoke --seed 2026

test: smoke $(TEST_BINARIES)
	! "$(ROGUELIKE)" --seed nope
	! "$(ROGUELIKE)" --seed -1 --smoke
	! "$(ROGUELIKE)" --seed 1 --seed 2 --smoke
	! "$(FIELDZERO)" --seed nope
	! "$(FIELDZERO)" --seed -1 --smoke
	! "$(FIELDZERO)" --seed 1 --seed 2 --smoke
	"$(FIELDZERO_REGRESSION)"
	"$(SCENE_TEST)"
	"$(CORE_TEST)"
	"$(INPUT_TEST)"
	"$(TERMINAL_TEST)"
	"$(FIELDZERO_PTY_TEST)" "$(FIELDZERO)"
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

package-test: $(LIBRARY) $(PKG_CONFIG_FILE)
	rm -rf "$(PACKAGE_TEST_ROOT)"
	$(MAKE) BUILD_DIR="$(BUILD_DIR)" DESTDIR="$(PACKAGE_TEST_STAGE)" PREFIX="$(PREFIX)" INSTALL_LIBDIR="$(INSTALL_LIBDIR)" INSTALL_INCLUDEDIR="$(INSTALL_INCLUDEDIR)" INSTALL_DATADIR="$(INSTALL_DATADIR)" install
	test -f "$(PACKAGE_TEST_STAGE)$(PREFIX)/$(INSTALL_LIBDIR)/libaforc.a"
	test -f "$(PACKAGE_TEST_STAGE)$(PREFIX)/$(INSTALL_INCLUDEDIR)/aforc/aforc.h"
	test -f "$(PACKAGE_TEST_STAGE)$(PREFIX)/$(INSTALL_LIBDIR)/pkgconfig/aforc.pc"
	cmp -s LICENSE "$(PACKAGE_TEST_STAGE)$(PREFIX)/$(INSTALL_DATADIR)/licenses/aforc/LICENSE"
	cmp -s THIRD_PARTY_NOTICES.md "$(PACKAGE_TEST_STAGE)$(PREFIX)/$(INSTALL_DATADIR)/licenses/aforc/THIRD_PARTY_NOTICES.md"
	test "$$(PKG_CONFIG_PATH="$(PACKAGE_TEST_STAGE)$(PREFIX)/$(INSTALL_LIBDIR)/pkgconfig" $(PKG_CONFIG) --modversion aforc)" = "$(AFORC_VERSION)"
	PKG_CONFIG_PATH="$(PACKAGE_TEST_STAGE)$(PREFIX)/$(INSTALL_LIBDIR)/pkgconfig" $(MAKE) -C tests/package_consumer BUILD_DIR="$(PACKAGE_TEST_CONSUMER_BUILD)" CC="$(CC)" PKG_CONFIG="$(PKG_CONFIG)" all test

run: $(ROGUELIKE)
	"$(ROGUELIKE)"

run-fieldzero: $(FIELDZERO)
	"$(FIELDZERO)"

install: $(LIBRARY) $(PKG_CONFIG_FILE)
	install -d \
		"$(DESTDIR)$(PREFIX)/$(INSTALL_LIBDIR)" \
		"$(DESTDIR)$(PREFIX)/$(INSTALL_LIBDIR)/pkgconfig" \
		"$(DESTDIR)$(PREFIX)/$(INSTALL_INCLUDEDIR)" \
		"$(DESTDIR)$(PREFIX)/$(INSTALL_DATADIR)/licenses/aforc"
	install -m 0644 "$(LIBRARY)" "$(DESTDIR)$(PREFIX)/$(INSTALL_LIBDIR)/libaforc.a"
	cp -R include/aforc "$(DESTDIR)$(PREFIX)/$(INSTALL_INCLUDEDIR)/"
	install -m 0644 "$(PKG_CONFIG_FILE)" "$(DESTDIR)$(PREFIX)/$(INSTALL_LIBDIR)/pkgconfig/aforc.pc"
	install -m 0644 LICENSE THIRD_PARTY_NOTICES.md "$(DESTDIR)$(PREFIX)/$(INSTALL_DATADIR)/licenses/aforc/"

clean:
	rm -rf "$(BUILD_DIR)"

help:
	@printf '%s\n' \
		'all       Build the static library and both examples (default)' \
		'example   Build the roguelike and FIELD ZERO examples' \
		'debug     Build unoptimized binaries with debug symbols' \
		'release   Build optimized binaries' \
		'strict    Rebuild with warnings promoted to errors, then test' \
		'sanitize  Rebuild with ASan/UBSan, then test' \
		'smoke     Run both deterministic, non-interactive example smokes' \
		'test      Run example CLI checks, smoke, and regression tests' \
		'package-test Stage the Make install and run its pkg-config consumer' \
		'run       Launch the playable terminal roguelike' \
		'run-fieldzero Launch FIELD ZERO' \
		'install   Install the library, headers, metadata, and licenses' \
		'HARDEN=0  Disable supported build hardening (default: 1)' \
		'clean     Remove Make build artifacts'

-include $(DEPS) $(ROGUELIKE_DEPS) $(FIELDZERO_DEPS) $(TEST_DEPS)
