TARGET = remotejoy
OBJS = main.o psplink.o USBHostFS.o exports.o

# Use the kernel's small inbuilt libc
USE_KERNEL_LIBC = 1
# Use only kernel libraries
USE_KERNEL_LIBS = 1

INCDIR = 
#CFLAGS = -O2 -G0 -Wall -fno-builtin-printf -DDEBUG
CFLAGS = -Os -G0 -Wall -fno-builtin-printf -I../../usbhostfs -I../../psplink
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =

LIBS = -lpsppower_driver

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
