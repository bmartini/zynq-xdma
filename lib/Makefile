CC = gcc
LDFLAGS := -lxdma
CFLAGS := -c -Wall -Werror
INCLUDES := -I. -I../dev


.PHONY : all
all : libxdma


libxdma.o :
	$(CC) $(CFLAGS) $(INCLUDES) -fpic libxdma.c


libxdma : libxdma.o
	$(CC) -shared -o libxdma.so libxdma.o


install : libxdma
	sudo cp ${PWD}/libxdma.so /usr/lib
	sudo cp ${PWD}/libxdma.h /usr/include
	sudo chmod 0755 /usr/lib/libxdma.so
	sudo ldconfig


uninstall :
	sudo rm /usr/lib/libxdma.so
	sudo rm /usr/include/libxdma.h
	sudo ldconfig


.PHONY : clean
clean :
	rm -f *.o libxdma.so
