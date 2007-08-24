OUTPUT=usbhostfs_pc
OBJS=main.o
LIBS=-lusb -lpthread
CFLAGS=-Wall -ggdb -I../usbhostfs -DPC_SIDE -D_FILE_OFFSET_BITS=64 -I. -O2
LDFLAGS=-L.

ifdef BUILD_BIGENDIAN
CFLAGS += -DBUILD_BIGENDIAN
endif

ifdef READLINE_SHELL
CFLAGS += -DREADLINE_SHELL
LIBS += -lreadline -lcurses
endif

ifdef BUILD_WIN32
CFLAGS  += -I../windows/lib -Wno-format
LDFLAGS += -L../windows/lib
OUTPUT := $(OUTPUT).exe
endif

PREFIX=$(shell psp-config --pspdev-path 2> /dev/null)

all: $(OUTPUT)

clean:
	rm -f $(OUTPUT) *.o

$(OUTPUT): $(OBJS)
	$(LINK.c) $(LDFLAGS) -o $@ $^ $(LIBS)

install: $(OUTPUT)
	@echo "Installing $(OUTPUT)..."
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/bin && cp $(OUTPUT) $(PREFIX)/bin; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@echo "Done!"
