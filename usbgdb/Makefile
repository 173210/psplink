TARGET = usbgdb
OBJS = main.o gdb-stub.o USBHostFS.o psplink.o psplinkUser.o

PRX_EXPORTS=exports.exp
BUILD_PRX=1
USE_USER_LIBS=1

INCDIR = 
CFLAGS = -O0 -G0 -Wall -g -I../modnet/
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS=

LIBDIR =

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
