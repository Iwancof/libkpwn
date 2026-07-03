KVER ?= 7.0.12-arch1-1
KDIR := /usr/lib/modules/$(KVER)/build

obj-m := vuln.o

all:
	make -C $(KDIR) M=$(CURDIR) modules

clean:
	make -C $(KDIR) M=$(CURDIR) clean
