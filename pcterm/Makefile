OUTPUT=pcterm
OBJS=pcterm.o

CFLAGS=-Wall -g -DSERIAL_SUPPORT
LIBS=-lreadline

all: pcterm

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OUTPUT) *.o
