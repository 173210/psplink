TARGET = conshell
OBJS = main.o psplink.o psplinkUser.o

PRX_EXPORTS=exports.exp
BUILD_PRX=1
USE_PSPSDK_LIBC=1
#USE_KERNEL_LIBS=1

INCDIR = 
CFLAGS = -O0 -G0 -Wall -g
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS=-lpspctrl

LIBDIR =

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
