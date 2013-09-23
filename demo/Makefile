CC = gcc
LDFLAGS :=
CFLAGS := -c -Wall
INCLUDES := -I../dev

EXECUTABLE = \
	demo \
	test

SOURCES = \
	xdma-demo.c \
	xdma-test.c

OBJECTS = $(SOURCES:.c=.o)

.PHONY : all
all: $(EXECUTABLE)


test : xdma-test.o
	$(CC) $< -o $@ $(LDFLAGS)


demo : xdma-demo.o
	$(CC) $< -o $@ $(LDFLAGS)


.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@


.PHONY : clean
clean:
	rm -f *.o $(EXECUTABLE)
