/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * main.c - Main code for network GDB Server
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $Id: main.c 1789 2006-02-05 18:17:47Z tyranid $
 * $HeadURL: svn://tyranid@svn.pspdev.org/psp/trunk/psplink/netgdb/main.c $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <modnet.h>
#include "usbgdb.h"
#include "../psplink_user/psplink_user.h"

#define MODULE_NAME "GDBServer"

PSP_MODULE_INFO(MODULE_NAME, 0, 1, 1);
PSP_MAIN_THREAD_NAME("GDBServer");

struct GdbContext g_context;

int usb_read_async_data(unsigned int chan, unsigned char *data, int len);
int usb_write_async_data(unsigned int chan, const void *data, int size);
void usb_async_flush(unsigned int chan);

/* Entry for GDB handler from psplink_user */
static int GdbTrapEntry(PspDebugRegBlock *regs)
{
	SceKernelThreadInfo info;
	u32 bits;
	int intc;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	if(sceKernelReferThreadStatus(sceKernelGetThreadId(), &info) < 0)
	{
		return 0;
	}

	/* Check if this is a thread from our debugged application */
	if((regs->epc < g_context.info.text_addr) 
			|| (regs->epc > (g_context.info.text_addr + g_context.info.text_size)))
	{
		return 0;
	}

	intc = pspSdkDisableInterrupts();
	memcpy(&g_context.regs, regs, sizeof(g_context.regs));
	pspSdkEnableInterrupts(intc);
	
	if(sceKernelSetEventFlag(g_context.evid, EVENT_HANDLE_EXP) < 0)
	{
		return 0;
	}

	if(sceKernelWaitEventFlag(g_context.evid, EVENT_CONTINUE, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, &bits, NULL) < 0)
	{
		return 0;
	}

	return 1;
}

/* These should be changed on for different remote methods */
int _gdbSupportLibReadByte(unsigned char *address, unsigned char *dest)
{
	u32 addr;

	addr = (u32) address;
	if((addr >= 0x08800000) && (addr < 0x0a000000))
	{
		*dest = *address;
		return 1;
	}

	return 0;
}

int _gdbSupportLibWriteByte(char val, unsigned char *dest)
{
	u32 addr;

	addr = (u32) dest;
	if((addr >= 0x08800000) && (addr < 0x0a000000))
	{
		*dest = val;
		return 1;
	}

	return 0;
}

int putDebugChar(unsigned char ch)
{
	return usb_write_async_data(1, &ch, 1);
}

int getDebugChar(unsigned char *ch)
{
	int ret = 0;

	*ch = 0;

	do
	{
		ret = usb_read_async_data(1, ch, 1);
	}
	while(ret < 1);

	return ret;
}

int writeDebugData(void *data, int len)
{
	return usb_write_async_data(1, data, len);
}

/* Start a simple tcp echo server */
void start_server()
{
	int firstrun = 1;
	u32 bits;

	while(1)
	{
		/* Should suspend all threads in the application, except perhaps the one we came from */
		if(GdbHandleException(&g_context.regs) == 0)
		{
			break;
		}

		if(!firstrun)
		{
			if(sceKernelSetEventFlag(g_context.evid, EVENT_CONTINUE) < 0)
			{
				printf(MODULE_NAME ": Error setting event flag\n");
				break;
			}
		}
		else
		{
			firstrun = 0;
		}

		/* Perhaps set a timeout and poll the socket to see if we have been sent a break */
		if(sceKernelWaitEventFlag(g_context.evid, EVENT_HANDLE_EXP, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, &bits, NULL) < 0)
		{
			printf(MODULE_NAME ": Error waiting on event flag\n");
			break;
		}
	}
}

/* Simple thread */
int main(int argc, char **argv)
{
	char *ext;

	printf("PSPLink USB GDBServer (c) 2k6 TyRaNiD/Lovely2\n");
	memset(&g_context, 0, sizeof(g_context));
	g_context.evid = -1;
	g_context.main_thread = sceKernelGetThreadId();
	GdbStubInit();

	if(argc < 2)
	{
		printf("usage: usbgdb.prx program.elf [args]\n");
		return 1;
	}

	g_context.evid = sceKernelCreateEventFlag("GdbEvent", PSP_EVENT_WAITMULTIPLE, 0, NULL);
	if(g_context.evid < 0)
	{
		printf(MODULE_NAME ": Error creating event flag 0x%08X\n", g_context.evid);
	}

	ext = strrchr(argv[1], '.');
	if(ext)
	{
		if(strcasecmp(ext, ".elf") == 0)
		{
			g_context.elf = 1;
		}
	}

	g_context.uid = sceKernelLoadModule(argv[1], 0, NULL);
	if(g_context.uid < 0)
	{
		printf(MODULE_NAME ": Could not load %s - 0x%08X\n", argv[1], g_context.uid);
		return 1;
	}

	printf(MODULE_NAME ": Loaded %s - UID 0x%08X\n", argv[1], g_context.uid);

	g_context.info.size = sizeof(g_context.info);
	if(psplinkReferModule(g_context.uid, &g_context.info) == 0)
	{
		printf(MODULE_NAME ": Could not get module information\n");
		return 1;
	}

	g_context.regs.epc = g_context.info.entry_addr;
	g_context.regs.cause = 9 << 2;
	g_context.argc = argc - 1;
	g_context.argv = &argv[1];

	psplinkUserRegisterGdbHandler(GdbTrapEntry);
	start_server();

	g_context.main_thread = -1;

	return 0;
}

int module_stop(SceSize args, void *argp)
{
	psplinkUserRegisterGdbHandler(NULL);

	if(g_context.main_thread >= 0)
	{
		sceKernelTerminateDeleteThread(g_context.main_thread);
		g_context.main_thread = -1;
	}

	if(g_context.evid >= 0)
	{
		sceKernelDeleteEventFlag(g_context.evid);
	}

	return 0;
}
