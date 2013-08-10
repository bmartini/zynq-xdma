export

all:
	make -C dev
	make -C demo

clean:
	make -C dev clean
	make -C demo clean
