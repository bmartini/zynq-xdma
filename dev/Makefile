obj-m += xdma.o
CC=${CROSS_COMPILE}gcc

# Path to the Linux kernel, if not passed in as arg, set default.
ifeq ($(KDIR),)
	KDIR := /lib/modules/$(shell uname -r)/build
endif


all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
