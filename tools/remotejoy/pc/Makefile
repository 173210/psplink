OUTPUT=remotejoy
OBJS=remotejoy.o

CFLAGS=-Wall -g

all: remotejoy

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ 

clean:
	rm -f $(OUTPUT) *.o
