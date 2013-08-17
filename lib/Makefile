CC = gcc
LDFLAGS := -lxdma
CFLAGS := -c -Wall -Werror
INCLUDES := -I../dev


all : libxdma


.PHONY : libxdma.o
libxdma.o :
	$(CC) $(CFLAGS) $(INCLUDES) -fpic libxdma.c


.PHONY : libxdma
libxdma : libxdma.o
	$(CC) -shared -o libxdma.so libxdma.o


.PHONY : app.o
app.o :
	$(CC) $(CFLAGS) $(INCLUDES) xdma-app.c


.PHONY : app
app : app.o
	$(CC) -o app xdma-app.o $(LDFLAGS)


install : libxdma
	sudo cp ${PWD}/libxdma.so /usr/lib
	sudo chmod 0755 /usr/lib/libxdma.so
	sudo ldconfig


uninstall :
	sudo rm /usr/lib/libxdma.so
	sudo ldconfig


.PHONY : clean
clean :
	rm -f *.o libxdma.so app
