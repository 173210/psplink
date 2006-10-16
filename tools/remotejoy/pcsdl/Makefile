OUTPUT=remotejoy
OBJS=remotejoy.o

CFLAGS=-Wall -g $(shell sdl-config --cflags)

all: remotejoy

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(shell sdl-config --libs)

clean:
	rm -f $(OUTPUT) *.o
