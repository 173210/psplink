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
#include "psplink_user.h"
#include "../psplink/version.h"

PSP_MODULE_INFO("PSPLINK_USER", 0, 1, 1);
PSP_MAIN_THREAD_PARAMS(0x20, 64, PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_NAME("PsplinkUser");

int psplinkHandleException(PspDebugRegBlock *regs);
void pspDebugResumeFromException(void);

static GdbHandler g_gdbhandler = NULL;

void ExceptionHandler(PspDebugRegBlock *regs)
{
	PspDebugRegBlock regsave;
	int intc;
	int handled = 0;

	intc = pspSdkDisableInterrupts();
	/* Save regs onto the stack */
	memcpy(&regsave, regs, sizeof(regsave));
	pspSdkEnableInterrupts(intc);

	if(g_gdbhandler)
	{
		handled = g_gdbhandler(&regsave);
	}

	if(handled == 0)
	{
		psplinkHandleException(&regsave);
	}

	intc = pspSdkDisableInterrupts();
	memcpy(regs, &regsave, sizeof(regsave));
	pspSdkEnableInterrupts(intc);

	pspDebugResumeFromException();
}

void psplinkUserRegisterGdbHandler(GdbHandler gdbhandler)
{
	g_gdbhandler = gdbhandler;
}

/* Simple thread */
int main(int argc, char **argv)
{
	pspDebugScreenInit();
	pspDebugInstallErrorHandler(ExceptionHandler);
	pspDebugScreenPrintf("PSPLINK User Module v%s\n", PSPLINK_VERSION);
	sceKernelExitDeleteThread(0);

	return 0;
}

int module_stop(int args, void *argp)
{
	return 0;
}
