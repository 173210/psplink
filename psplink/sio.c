/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * sio.c - PSPLINK kernel module sio code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 * $HeadURL$
 * $Id$
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspumd.h>
#include <psputilsforkernel.h>
#include "psplink.h"

void stdoutInit(void);

static SceUID g_eventflag = -1;

static int intr_handler(void *arg)
{
	u32 stat;

	stat = _lw(0xBE500040);
	_sw(stat, 0xBE500044);

	sceKernelDisableIntr(PSP_HPREMOTE_INT);

	sceKernelSetEventFlag(g_eventflag, EVENT_SIO);

	return -1;
}

/* Read a character with a timeout */
int sioReadCharWithTimeout(void)
{
	int ch;
	u32 result;
	u32 timeout;

	timeout = 500000;
	ch = pspDebugSioGetchar();
	if(ch == -1)
	{
		sceKernelEnableIntr(PSP_HPREMOTE_INT);
		sceKernelWaitEventFlag(g_eventflag, EVENT_SIO, 0x21, &result, &timeout);
		ch = pspDebugSioGetchar();
	}

	return ch;
}

int sioReadChar(void)
{
	int ch;
	u32 result;

	ch = pspDebugSioGetchar();
	if(ch == -1)
	{
		sceKernelEnableIntr(PSP_HPREMOTE_INT);
		sceKernelWaitEventFlag(g_eventflag, EVENT_SIO, 0x21, &result, NULL);

		ch = pspDebugSioGetchar();
	}

	return ch;
}

void sioInit(void)
{
	g_eventflag = sceKernelCreateEventFlag("SioShellEvent", 0, 0, 0);
	pspDebugSioInit();
	pspDebugSioSetBaud(115200);
	stdoutInit();
	pspDebugSioInstallKprintf();
	sceKernelRegisterIntrHandler(PSP_HPREMOTE_INT, 1, intr_handler, NULL, NULL);
	sceKernelEnableIntr(PSP_HPREMOTE_INT);
}
