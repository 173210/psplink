/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * tty.c - PSPLINK kernel module tty code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <pspkerror.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspumd.h>
#include <psputilsforkernel.h>
#include "psplink.h"
#include "apihook.h"
#include "util.h"

#define STDIN_BUFSIZE 4096

static PspDebugPrintHandler g_sioHandler = NULL;
static PspDebugPrintHandler g_wifiHandler = NULL;
static PspDebugPrintHandler g_usbHandler = NULL;

/* STDIN buffer */
static char g_stdinbuf[STDIN_BUFSIZE];
/* Position in STDIN buffer */
static int g_stdinreadpos = 0;
static int g_stdinwritepos = 0;
static int g_stdinsizeleft = STDIN_BUFSIZE;
/* The waiting thread for stdin data */
static SceUID g_stdinwaitth = -1;

static int outputHandler(const char *data, int size)
{
	if(g_sioHandler)
	{
		g_sioHandler(data, size);
	}

	if(g_wifiHandler)
	{
		g_wifiHandler(data, size);
	}

	if(g_usbHandler)
	{
		g_usbHandler(data, size);
	}

	return size;
}

static int inputHandler(char *data, int size)
{
	if(g_stdinwaitth >= 0)
	{
	}

	return 0;
}

void ttySetWifiHandler(PspDebugPrintHandler wifiHandler)
{
	g_wifiHandler = wifiHandler;
}

void ttySetSioHandler(PspDebugPrintHandler sioHandler)
{
	g_sioHandler = sioHandler;
}

void ttySetUsbHandler(PspDebugPrintHandler usbHandler)
{
	g_usbHandler = usbHandler;
}

void ttyAddInputData(void *data, int size)
{
	int sizestart;
	int sizeend;
	int sizewrite;
	int intc;

	intc = pspSdkDisableInterrupts();
	if(g_stdinsizeleft > 0)
	{
		if(size > g_stdinsizeleft)
		{
			/* Ensure size is at most the amount we have left */
			size = g_stdinsizeleft;
		}

		if(g_stdinwritepos < g_stdinreadpos)
		{
			sizestart = g_stdinsizeleft;
			sizeend = 0;
		}
		else
		{
			sizestart = STDIN_BUFSIZE - g_stdinwritepos;
			sizeend = g_stdinreadpos;
		}

		/* Copy in from the writepos to end of the buf */
		sizewrite = size < sizestart ? size : sizestart;
		memcpy(&g_stdinbuf[g_stdinwritepos], data, sizewrite);
		g_stdinwritepos = (g_stdinwritepos + sizewrite) % STDIN_BUFSIZE;
		data += sizewrite;
		size -= sizewrite;
		g_stdinsizeleft -= sizewrite;
		/* If the start wasn't sufficient the copy into the other end of the buffer */
		if(size > 0)
		{
			sizewrite = size < sizeend ? size : sizeend;
			memcpy(&g_stdinbuf[g_stdinwritepos], data, sizewrite);
			g_stdinwritepos = (g_stdinwritepos + sizewrite) % STDIN_BUFSIZE;
			g_stdinsizeleft -= sizewrite;
		}
	}

	if(g_stdinwaitth >= 0)
	{
		sceKernelWakeupThread(g_stdinwaitth);
		g_stdinwaitth = -1;
	}

	pspSdkEnableInterrupts(intc);
}

static int close_func(int fd)
{
	int ret = SCE_KERNEL_ERROR_FILEERR;

	if(fd > 2)
	{
		ret = sceIoClose(fd);
	}

	return ret;
}

void ttyInit(void)
{
	pspDebugInstallStdoutHandler(outputHandler);
	pspDebugInstallStderrHandler(outputHandler);
	pspDebugInstallStdinHandler(inputHandler);
	/* Install a patch to prevent a naughty app from closing stdout */
	apiHookByNid(refer_module_by_name("sceIOFileManager", NULL), "IoFileMgrForUser", 0x810c4bc3, close_func);
}
