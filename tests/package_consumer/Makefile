# AForc installed-package Make consumer
# SPDX-License-Identifier: MIT

CC ?= cc
PKG_CONFIG ?= pkg-config
BUILD_DIR ?= build
AFORC_REQUIRED_VERSION ?= 0.1

CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror
TARGET := $(BUILD_DIR)/aforc_package_consumer

.DEFAULT_GOAL := all

.PHONY: all test clean

all: $(TARGET)

$(TARGET): main.c
	@mkdir -p "$(@D)"
	$(PKG_CONFIG) --atleast-version="$(AFORC_REQUIRED_VERSION)" aforc
	$(CC) $(CPPFLAGS) $(CFLAGS) $$($(PKG_CONFIG) --cflags aforc) "$<" $(LDFLAGS) $$($(PKG_CONFIG) --libs aforc) $(LDLIBS) -o "$@"

test: $(TARGET)
	"$(TARGET)"

clean:
	rm -rf "$(BUILD_DIR)"
