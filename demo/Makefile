CC = gcc
LDFLAGS := -lxdma
CFLAGS := -c -Wall
INCLUDES := -I. -I../dev

EXECUTABLE = \
	app \
	demo \
	test


.PHONY : all
all: $(EXECUTABLE)


test : xdma-test.o
	$(CC) $< -o $@ $(LDFLAGS)


demo : xdma-demo.o
	$(CC) $< -o $@ $(LDFLAGS)


app : xdma-app.o
	$(CC) $< -o $@ $(LDFLAGS)


.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@


.PHONY : clean
clean:
	rm -f *.o $(EXECUTABLE)
