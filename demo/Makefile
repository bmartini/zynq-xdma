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

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $< -o $@

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm -f *.o $(EXECUTABLE)
