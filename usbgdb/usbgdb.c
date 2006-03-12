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

int usb_read_async_data(unsigned int chan, unsigned char *data, int len);
int usb_write_async_data(unsigned int chan, const void *data, int size);
void usb_async_flush(unsigned int chan);

int isInit(void)
{
	return 1;
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

void start_server(void)
{
	GdbMain();
}

void stop_server(void)
{
}
