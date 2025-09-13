LIBKPWN	:= /home/iwancof/WorkSpace/CTF/libkpwn

CC       := gcc
CFLAGS   := -O0 -ggdb3 -Wall -Wextra

ARCH := $(word 1,$(subst -, ,$(shell $(CC) -dumpmachine)))
KPWN_SRC	:= $(wildcard $(LIBKPWN)/src/*.c)	$(wildcard $(LIBKPWN)/src/$(ARCH)/*.c) $(wildcard $(LIBKPWN)/src/$(ARCH)/*.s)

# for criterion
TEST_CC	:= gcc
TEST_CFLAGS	:= -O0 -ggdb3 -Wall -Wextra
TEST_ARCH := $(word 1,$(subst -, ,$(shell $(CC) -dumpmachine)))
TEST_SRC	:= $(wildcard $(LIBKPWN)/src/*.c)	$(wildcard $(LIBKPWN)/src/$(ARCH)/*.c) $(wildcard $(LIBKPWN)/src/$(ARCH)/*.s) $(wildcard $(LIBKPWN)/tests/*.c)

.PHONY: all clean

demo: demo.c $(KPWN_SRC)
	$(CC) $(CFLAGS) $^ -o $@

run_test: $(TEST_SRC)
	$(TEST_CC) $(TEST_CFLAGS) $^ -o $@ -lcriterion
	./$@
	
clean:
	rm -f demo
	rm -f run_test
