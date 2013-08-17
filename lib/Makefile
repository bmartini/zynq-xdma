CC = gcc
LDFLAGS :=
CFLAGS := -c -Wall
INCLUDES := -I../dev

EXECUTABLE = \
	app

SOURCES = \
	xdma-app.c

OBJECTS = $(SOURCES:.c=.o)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $< -o $@

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm -f *.o $(EXECUTABLE)
