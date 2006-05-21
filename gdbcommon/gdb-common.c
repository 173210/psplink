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
int GdbTrapEntry(struct PsplinkContext *ctx)
{
	SceKernelThreadInfo info;
	struct ExceptionMsg msg;
	u32 bits;
	//int intc;

	memset(&msg, 0, sizeof(msg));
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	if(sceKernelReferThreadStatus(ctx->thid, &info) < 0)
	{
		return 0;
	}

	/* Check if this is a thread from our debugged application */
	if(((u32) info.entry < g_context.info.text_addr) 
			|| ((u32) info.entry >= (g_context.info.text_addr + g_context.info.text_size)))
	{
		return 0;
	}

	memcpy(&g_context.ctx, ctx, sizeof(g_context.ctx));

	msg.ctx = ctx;

	if(sceKernelSetEventFlag(g_context.evid, EVENT_HANDLE_EXP) < 0)
	{
		return 0;
	}

	if(sceKernelWaitEventFlag(g_context.evid, EVENT_CONTINUE, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, &bits, NULL) < 0)
	{
		return 0;
	}

	memcpy(ctx, &g_context.ctx, sizeof(g_context.ctx));

	return 1;
}

void GdbMain(void)
{
	int firstrun = 1;
	u32 bits;
	//struct ExceptionMsg *msg;

	while(1)
	{
		/* Should suspend all threads in the application, except perhaps the one we came from */
		if(GdbHandleException(&g_context.ctx) == 0)
		{
			break;
		}

		if(!firstrun)
		{
			if(sceKernelSetEventFlag(g_context.evid, EVENT_CONTINUE) < 0)
			{
				printf(GDB_MODULE_NAME ": Error setting event flag\n");
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
			printf(GDB_MODULE_NAME ": Error waiting on event flag\n");
			break;
		}
	}
}

/* These should be changed on for different remote methods */
int GdbReadByte(unsigned char *address, unsigned char *dest)
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

int GdbWriteByte(char val, unsigned char *dest)
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
