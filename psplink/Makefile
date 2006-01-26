TARGET = psplink
OBJS = main.o shell.o config.o bitmap.o sio.o stdout.o pspdis.o decodeaddr.o memoryUID.o kmode.o exception.o parse_args.o psplinkcnf.o util.o script.o debug.o symbols.o exports.o

# Use the kernel's small inbuilt libc
USE_KERNEL_LIBC = 1
#USE_PSPSDK_LIBC = 1
# Use only kernel libraries
USE_KERNEL_LIBS = 1

INCDIR = 
#CFLAGS = -O2 -G0 -Wall -DDEBUG
CFLAGS = -O2 -G0 -Wall -fno-builtin-printf
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =

LIBS = -lpspusb_driver -lpspusbstor -lpspumd

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

LIBS += -lpsphprm_driver 
