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
PSP_MAIN_THREAD_PARAMS(0x20, 64, PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_NAME("PsplinkUser");

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
int main(int argc, char **argv)
{
	pspDebugInstallErrorHandler(ExceptionHandler);
	Kprintf("PSPLINKUSER loaded\n");
	sceKernelExitDeleteThread(0);

	return 0;
}
