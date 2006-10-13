TARGET = usbgdb
OBJS = usbgdb.o USBHostFS.o psplinkUser.o

PRX_EXPORTS=exports.exp
BUILD_PRX=1
USE_USER_LIBS=1

INCDIR = 
CFLAGS = -O0 -G0 -Wall -g -I../gdbcommon/ -I../usbhostfs -I../libpsplink
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS = -L../gdbcommon/ -L../libpsplink

LIBS=-lgdb -lpsplink

LIBDIR =

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
