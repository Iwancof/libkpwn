# Exploit-project template Makefile.
#
# Point LIBKPWN at your libkpwn checkout. When this template lives inside the
# repo (libkpwn/template/) the default `..` works; copy it into a challenge dir
# and override, e.g. `make LIBKPWN=~/ctf/libkpwn` or edit the line below.
LIBKPWN ?= ..

# musl-gcc yields a small, dependency-free static binary ideal for dropping into
# an initramfs. If musl-gcc is unavailable, fall back to the host gcc: build with
#   make CC=gcc
CC     ?= musl-gcc
CFLAGS := -static -Wall -Wextra -Os

# Aggressive size reduction for the payload binary.
MINIMIZE := -flto -fno-plt -fdata-sections -ffunction-sections \
            -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants \
            -Wl,--gc-sections

include $(LIBKPWN)/kpwn.mk

.PHONY: all clean
all: main

main: main.c $(KPWN_SRCS)
	$(CC) $(CFLAGS) $(KPWN_CFLAGS) $(MINIMIZE) $^ -o $@

# Stripped + UPX-packed variant (needs `upx`).
cpmain: main
	cp $< $@
	strip --strip-all $@
	upx --brute $@

### Repack the target initramfs after building (challenge-specific).
### Adjust the rootfs path/layout, then uncomment:
# repack: main
# 	cp main ../rootfs/main && chmod +x ../rootfs/main
# 	cd ../rootfs && find . -print0 \
# 	  | cpio -o --format=newc --null --owner=root \
# 	  | gzip > ../updated.cpio.gz

clean:
	rm -f main cpmain
