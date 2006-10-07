TARGET = psplink
OBJS = main.o shell.o config.o bitmap.o sio.o tty.o disasm.o decodeaddr.o memoryUID.o kmode.o exception.o parse_args.o psplinkcnf.o util.o script.o debug.o symbols.o libs.o apihook.o thctx.o stdio.o exports.o

# Use the kernel's small inbuilt libc
USE_KERNEL_LIBC = 1
#USE_PSPSDK_LIBC = 1
# Use only kernel libraries
USE_KERNEL_LIBS = 1


INCDIR = ../usbhostfs
CFLAGS = -Os -G0 -Wall -fno-builtin-printf
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS=-nodefaultlibs

USB_ONLY=1

ifdef USB_ONLY
CFLAGS += -DUSB_ONLY
endif

LIBDIR =

LIBS = -lpspusb_driver -lpspusbstor -lpspumd

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

LIBS += -lpsphprm_driver -lpsppower_driver -lpspdisplay_driver
