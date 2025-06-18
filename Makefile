CC       := musl-gcc
CFLAGS   := -O0 -ggdb3 -Wall -Wextra

.PHONY: all clean

demo: demo.c $(wildcard src/*.c) $(wildcard src/*.s)
	@echo "LINK demo"
	$(CC) $^ -o $@
	
clean:
	rm -f demo