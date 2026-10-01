# Linux Kernel Module Makefile for ProcessPilot Pro

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod modules.order Module.symvers .*.cmd

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

.PHONY: default clean install
