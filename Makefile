obj-m += xdma.o

all:
	make -C ../../.. M=$(PWD) modules

clean:
	make -C ../../.. M=$(PWD) clean
