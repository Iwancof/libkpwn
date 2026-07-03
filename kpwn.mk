# kpwn.mk — the single fragment a consumer includes to build against libkpwn.
#
# Usage (from an exploit project's Makefile):
#
#     CC      := musl-gcc            # set CC *before* including (arch is probed via CC)
#     LIBKPWN := ../libkpwn          # path to your libkpwn checkout (relative or absolute)
#     include $(LIBKPWN)/kpwn.mk
#
#     main: main.c $(KPWN_SRCS)
#     	$(CC) $(CFLAGS) $(KPWN_CFLAGS) $^ -o $@
#
# This file self-locates via $(lastword $(MAKEFILE_LIST)), so LIBKPWN may be a
# relative path and the whole tree stays relocatable — no absolute paths baked in.
#
# Exports:
#   KPWN_DIR      absolute-or-relative root of the libkpwn checkout
#   KPWN_INCLUDE  the include/ dir (pass with -I, see KPWN_CFLAGS)
#   KPWN_ARCH     probed target arch (x86_64 / aarch64 / ...)
#   KPWN_SRCS     arch-filtered list of library .c/.s sources to compile in
#   KPWN_CFLAGS   flags a consumer must add to CFLAGS (currently -I$(KPWN_INCLUDE))

KPWN_DIR     := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
KPWN_INCLUDE := $(KPWN_DIR)/include

# Probe the target arch from the compiler triple (e.g. x86_64-pc-linux-gnu -> x86_64).
KPWN_CC      ?= $(CC)
KPWN_ARCH    := $(word 1,$(subst -, ,$(shell $(KPWN_CC) -dumpmachine)))

KPWN_SRCS := $(wildcard $(KPWN_DIR)/src/*.c) \
             $(wildcard $(KPWN_DIR)/src/$(KPWN_ARCH)/*.c) \
             $(wildcard $(KPWN_DIR)/src/$(KPWN_ARCH)/*.s)

KPWN_CFLAGS := -I$(KPWN_INCLUDE)
