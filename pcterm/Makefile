OUTPUT=pcterm
OBJS=pcterm.o

CFLAGS=-Wall -g -DSERIAL_SUPPORT
LIBS=-lreadline -lcurses

PREFIX=$(shell psp-config --pspdev-path 2> /dev/null)

all: pcterm

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OUTPUT) *.o

install: $(OUTPUT)
	@echo "Installing $(OUTPUT)..."
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/bin && cp $(OUTPUT) $(PREFIX)/bin; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@echo "Done!"
