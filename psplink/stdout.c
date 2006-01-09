/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * stdout.c - PSPLINK kernel module stdout code
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

static u32 g_close[2];

PspDebugPrintHandler g_sioHandler = NULL;
PspDebugPrintHandler g_wifiHandler = NULL;

static void patch_close(void);

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

	return size;
}

void stdoutSetWifiHandler(PspDebugPrintHandler wifiHandler)
{
	g_wifiHandler = wifiHandler;
}

void stdoutSetSioHandler(PspDebugPrintHandler sioHandler)
{
	g_sioHandler = sioHandler;
}

static void unpatch_close(void)
{
	u32 *jump;
	u32 *patch;

	jump = (u32 *) sceIoClose;
	patch = (u32 *) (0x80000000 | ((*jump & 0x03FFFFFF) << 2));

	patch[0] = g_close[0];
	patch[1] = g_close[1];

	sceKernelDcacheWritebackAll();
	sceKernelIcacheInvalidateAll();
}

int close_func(int fd)
{
	int ret = SCE_KERNEL_ERROR_FILEERR;

	if(fd > 2)
	{
		unpatch_close();
		ret = sceIoClose(fd);
		patch_close();
	}

	return ret;
}

/* Do some kernel patching */
static void patch_close(void)
{
	u32 *jump;
	u32 *patch;

	jump = (u32 *) sceIoClose;
	patch = (u32 *) (0x80000000 | ((*jump & 0x03FFFFFF) << 2));

	g_close[0] = patch[0];
	g_close[1] = patch[1];

	/* Patch in a jump to the reset function */
	patch[0] = 0x08000000 | ((((u32) close_func) & 0x0FFFFFFF) >> 2);
	patch[1] = 0;
	sceKernelDcacheWritebackAll();
	sceKernelIcacheInvalidateAll();
}

void stdoutInit(void)
{
	pspDebugInstallStdoutHandler(outputHandler);
	pspDebugInstallStderrHandler(outputHandler);
	/* Install a patch to prevent a naughty app from closing stdout */
	patch_close();
}
