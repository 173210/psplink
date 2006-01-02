TARGET = netshell
OBJS = main.o psplink.o sdkbridge.o

PRX_EXPORTS=exports.exp
BUILD_PRX=1

INCDIR = 
CFLAGS = -O0 -G0 -Wall -g
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS=-lpspwlan

LIBDIR =

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
