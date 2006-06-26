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

all: $(OUTPUT)

clean:
	rm -f $(OUTPUT) *.o

$(OUTPUT): $(OBJS)
	$(LINK.c) $(LDFLAGS) -o $@ $^ $(LIBS)
