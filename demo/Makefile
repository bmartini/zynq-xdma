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
	$(CC) $(LDFLAGS) $< -o $@


demo : xdma-demo.o
	$(CC) $(LDFLAGS) $< -o $@


.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@


.PHONY : clean
clean:
	rm -f *.o $(EXECUTABLE)
