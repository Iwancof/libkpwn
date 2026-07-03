# libkpwn — development Makefile.
#
# Consumers do NOT use this file; they `include kpwn.mk` from their own project
# (see template/Makefile). This Makefile dogfoods kpwn.mk to build the demo, the
# test runner, and an optional static archive.

CC     := gcc
CFLAGS := -O0 -ggdb3 -Wall -Wextra

include kpwn.mk

BUILD := build

# Library object list, keeping the source path in the object name so that
# src/memory.c and src/$(ARCH)/memory.c do not collide.
LIB_SRCS := $(wildcard src/*.c) \
            $(wildcard src/$(KPWN_ARCH)/*.c) \
            $(wildcard src/$(KPWN_ARCH)/*.s)
LIB_OBJS := $(patsubst %,$(BUILD)/%.o,$(LIB_SRCS))

TEST_SRCS := $(KPWN_SRCS) $(filter-out tests/smoke_%.c,$(wildcard tests/*.c))

FORMAT_FILES := $(shell find src include tests template demo.c -name '*.c' -o -name '*.h' 2>/dev/null)

.PHONY: all lib test smoke format format-check clean
all: lib test

# --- static archive (built with the host toolchain) ---------------------------
lib: libkpwn.a
libkpwn.a: $(LIB_OBJS)
	ar rcs $@ $^

$(BUILD)/%.o: %
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(KPWN_CFLAGS) -c $< -o $@

# --- unit tests (zero external deps: tests/ktest.h) ---------------------------
test: run_test
run_test: $(TEST_SRCS)
	$(CC) $(CFLAGS) $(KPWN_CFLAGS) -Itests $^ -o $@
	./$@

# --- smoke tests (real kernel interaction, not run in CI) ---------------------
smoke: smoke_spray
smoke_spray: tests/smoke_spray.c $(KPWN_SRCS)
	$(CC) $(CFLAGS) $(KPWN_CFLAGS) $^ -o $@
	./$@

# --- demo (needs restored kernel primitives; see docs) ------------------------
demo: demo.c $(KPWN_SRCS)
	$(CC) $(CFLAGS) $(KPWN_CFLAGS) $^ -o $@

# --- formatting ---------------------------------------------------------------
format:
	clang-format -i $(FORMAT_FILES)
format-check:
	clang-format --dry-run -Werror $(FORMAT_FILES)

clean:
	rm -rf demo run_test libkpwn.a $(BUILD)
