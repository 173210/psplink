/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psplink_ex.c - Main code for PSPLINK user exceptions.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#include <pspkernel.h>
#include <string.h>
#include <stdio.h>
#include "psplink_user.h"
#include "psplink_ex.h"

struct PsplinkContext g_psplinkContext[PSPLINK_MAX_CONTEXT];
static GdbHandler g_gdbhandler = NULL;

void psplinkExceptionHandler(void);
int sceKernelRegisterDefaultExceptionHandler(void *func);
int psplinkHandleException(PspDebugRegBlock *regs);

void psplinkUserRegisterGdbHandler(GdbHandler gdbhandler)
{
	g_gdbhandler = gdbhandler;
}

/* Install an error handler */
int psplinkInitException(void)
{
	int i;
	memset(g_psplinkContext, 0, sizeof(g_psplinkContext));

	for(i = 0; i < (PSPLINK_MAX_CONTEXT-1); i++)
	{
		g_psplinkContext[i].pNext = &g_psplinkContext[i+1];
	}

	return sceKernelRegisterDefaultExceptionHandler((void *) psplinkExceptionHandler);
}

/**
  * The entry point for our exception "trap"
  */
void psplinkTrap(struct PsplinkContext *ctx)
{
	int handled = 0;
	SceUID thid;

	thid = sceKernelGetThreadId();

	if(ctx == NULL)
	{
		printf("No more free contexts for exception trap, deleting thread 0x%08X\n", thid);
		sceKernelExitDeleteThread(0);
	}

	ctx->thid = thid;

	if(g_gdbhandler)
	{
		handled = g_gdbhandler(ctx);
	}

	if(handled == 0)
	{
		psplinkHandleException((PspDebugRegBlock *) &ctx->regs);
	}

	psplinkResumeFromException(ctx);
}
