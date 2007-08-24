OUTPUT=remotejoy
OBJS=remotejoy.o

CFLAGS=-Wall -g

PREFIX=$(shell psp-config --pspdev-path 2> /dev/null)

all: remotejoy

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ 

clean:
	rm -f $(OUTPUT) *.o

install: $(OUTPUT)
	@echo "Installing $(OUTPUT)..."
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/bin && cp $(OUTPUT) $(PREFIX)/bin; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@echo "Done!"
