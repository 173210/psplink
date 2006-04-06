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
#include "libs.h"

#define STDIN_BUFSIZE 4096

static PspDebugPrintHandler g_sioHandler = NULL;
static PspDebugPrintHandler g_wifiHandler = NULL;
static PspDebugPrintHandler g_usbHandler = NULL;
static PspDebugPrintHandler g_consHandler = NULL;

/* STDIN buffer */
static char g_stdinbuf[STDIN_BUFSIZE];
/* Position in STDIN buffer */
static int g_stdinreadpos = 0;
static int g_stdinwritepos = 0;
static int g_stdinsize = 0;
/* The waiting thread for stdin data */
static SceUID g_stdinwaitth = -1;

extern struct GlobalContext g_context;

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

	if(g_consHandler)
	{
		if ((!g_context.inexec) || g_context.consinterfere)
		{
			g_consHandler(data, size);
		}
	}

	return size;
}

static int inputHandler(char *data, int size)
{
	int intc;
	int sizeread = 0;
	int i;

	while(1)
	{
		intc = pspSdkDisableInterrupts();
		sizeread = size < g_stdinsize ? size : g_stdinsize;
		for(i = 0; i < sizeread; i++)
		{
			*data++ = g_stdinbuf[g_stdinreadpos++];
			g_stdinreadpos %= STDIN_BUFSIZE;
			g_stdinsize--;
		}

		if(sizeread == 0)
		{
			g_stdinwaitth = sceKernelGetThreadId();
		}
		pspSdkEnableInterrupts(intc);

		if(sizeread > 0)
		{
			break;
		}

		sceKernelSleepThread();
	}

	return sizeread;
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

void ttySetConsHandler(PspDebugPrintHandler consHandler)
{
	g_consHandler = consHandler;
}

void ttyAddInputData(const char *data, int size)
{
	int intc;
	int sizeleft;

	intc = pspSdkDisableInterrupts();
	sizeleft = size < (STDIN_BUFSIZE - g_stdinsize) ? size : (STDIN_BUFSIZE - g_stdinsize);
	while(sizeleft > 0)
	{
		g_stdinbuf[g_stdinwritepos++] = *data++;
		g_stdinwritepos %= STDIN_BUFSIZE;
		g_stdinsize++;
		sizeleft--;
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
	SceUID uid;

	pspDebugInstallStdoutHandler(outputHandler);
	pspDebugInstallStderrHandler(outputHandler);
	pspDebugInstallStdinHandler(inputHandler);
	/* Install a patch to prevent a naughty app from closing stdout */
	uid = refer_module_by_name("sceIOFileManager", NULL);
	if(uid >= 0)
	{
		apiHookByNid(uid, "IoFileMgrForUser", 0x810c4bc3, close_func);
		libsPatchFunction(uid, "IoFileMgrForKernel", 0x3c54e908, 0xFFFF);
	}
}
