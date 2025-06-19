CC       := musl-gcc
CFLAGS   := -O0 -ggdb3 -Wall -Wextra

ARCH := $(word 1,$(subst -, ,$(shell $(CC) -dumpmachine)))
LIBKPWN	:= /home/iwancof/WorkSpace/CTF/libkpwn
KPWN_SRC	:= $(wildcard $(LIBKPWN)/src/*.c)	$(wildcard $(LIBKPWN)/src/$(ARCH)/*.c) $(wildcard $(LIBKPWN)/src/$(ARCH)/*.s)

.PHONY: all clean

demo: demo.c $(KPWN_SRC)
	@echo $(ARCH)
	@echo "LINK demo"
	$(CC) $^ -o $@
	
clean:
	rm -f demo
