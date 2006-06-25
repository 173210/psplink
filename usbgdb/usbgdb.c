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
#include <gdb-common.h>
#include <usbhostfs.h>
#include <usbasync.h>

struct AsyncEndpoint g_endp;

int isInit(void)
{
	return 1;
}

int putDebugChar(unsigned char ch)
{
	return usbAsyncWrite(ASYNC_GDB, &ch, 1);
}

int getDebugChar(unsigned char *ch)
{
	int ret = 0;

	*ch = 0;

	do
	{
		ret = usbAsyncRead(ASYNC_GDB, ch, 1);
	}
	while(ret < 1);

	return ret;
}

int writeDebugData(void *data, int len)
{
	return usbAsyncWrite(ASYNC_GDB, data, len);
}

void start_server(void)
{
	usbAsyncRegister(ASYNC_GDB, &g_endp);
	GdbMain();
}

void stop_server(void)
{
	usbAsyncUnregister(ASYNC_GDB);
}
