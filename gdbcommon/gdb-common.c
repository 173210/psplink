/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * gdb-common.c - Common code for GDB Server
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $Id$
 * $HeadURL$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "gdb-common.h"
#include "../psplink_user/psplink_user.h"

struct GdbContext g_context;

/* Entry for GDB handler from psplink_user */
int GdbTrapEntry(PspDebugRegBlock *regs)
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

void GdbMain(void)
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
