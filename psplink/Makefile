TARGET = psplink
OBJS = main.o memoryUID.o exception.o gdb-stub.o parse_args.o exports.o

# Use the kernel's small inbuilt libc
USE_KERNEL_LIBC = 1
#USE_PSPSDK_LIBC = 1
# Use only kernel libraries
USE_KERNEL_LIBS = 1

INCDIR = 
#CFLAGS = -O2 -G0 -Wall -DDEBUG
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =

LIBS = -lpspusb_driver -lpspusbstor -lpspumd

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

LIBS += -lpsphprm_driver 
