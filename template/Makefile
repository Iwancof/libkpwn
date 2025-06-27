LIBKPWN	:= /home/iwancof/WorkSpace/CTF/libkpwn

CC       := musl-gcc
CFLAGS   := -static -Wall -Wextra -Os
MINIMAZE	:= -flto -fno-plt -fdata-sections -ffunction-sections -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -Wl,--gc-sections

ARCH := $(word 1,$(subst -, ,$(shell $(CC) -dumpmachine)))
KPWN_SRC	:= $(wildcard $(LIBKPWN)/src/*.c)	$(wildcard $(LIBKPWN)/src/$(ARCH)/*.c) $(wildcard $(LIBKPWN)/src/$(ARCH)/*.s)

.PHONY: all clean

main: main.c $(KPWN_SRC)
	$(CC) $(CFLAGS) $(MINIMAZE) $^ -o $@ 

cpmain: main
	cp $< $@
	strip --strip-all $@
	upx --brute $@

### -- if you want to use --
# repack: main cpmain
# 	cp main ../rootfs/main
# 	cp cpmain ../rootfs/cpmain
# 	chmod +x ../rootfs/main
# 	chmod +x ../rootfs/cpmain
# 	cd ../rootfs && find . -print0 | cpio -o --format=newc --null --owner=root | gzip > ../updated.cpio.gz

clean:
	rm -f main cpmain
