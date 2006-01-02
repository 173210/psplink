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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspumd.h>
#include <psputilsforkernel.h>
#include "psplink.h"

PspDebugPrintHandler g_sioHandler = pspDebugSioPutText;
PspDebugPrintHandler g_wifiHandler = NULL;

static int outputHandler(const char *data, int size)
{
	g_sioHandler(data, size);
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

void stdoutInit(void)
{
	pspDebugInstallStdoutHandler(outputHandler);
	pspDebugInstallStderrHandler(outputHandler);
}
