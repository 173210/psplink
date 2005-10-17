/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - Main code for PSPLINK user module.
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

PSP_MODULE_INFO("PSPLINK_USER", 0, 1, 1);

int psplinkHandleException(PspDebugRegBlock *regs);
void pspDebugResumeFromException(void);

void ExceptionHandler(PspDebugRegBlock *regs)
{
	if(psplinkHandleException(regs))
	{
		pspDebugResumeFromException();
	}
	else
	{
		/* Don't resume */
		sceKernelSleepThread();
	}
}

/* Simple thread */
int main_thread(SceSize args, void *argp)
{
	pspDebugInstallErrorHandler(ExceptionHandler);
	Kprintf("PSPLINKUSER loaded\n");
	sceKernelSleepThread();

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("PspLinkUser", main_thread, 0x20, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, args, argp);
	}

	return 0;
}
