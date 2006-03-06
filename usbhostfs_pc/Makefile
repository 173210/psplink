OUTPUT=usbhostfs_pc
OBJS=main.o
LIBS=-lusb -lpthread
CFLAGS=-Wall -ggdb -I../usbhostfs -DPC_SIDE

ifdef BUILD_BIGENDIAN
CFLAGS += -DBUILD_BIGENDIAN
endif

all: $(OUTPUT)

clean:
	rm -f $(OUTPUT) *.o

$(OUTPUT): $(OBJS)
	$(LINK.c) $(LDFLAGS) -o $@ $^ $(LIBS)
