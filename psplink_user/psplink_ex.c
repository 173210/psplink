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
#include "../psplink/debug.h"
#include "psplink_user.h"
#include "psplink_ex.h"

struct PsplinkContext g_psplinkContext[PSPLINK_MAX_CONTEXT];
static GdbHandler g_gdbhandler = NULL;

void psplinkDefaultExHandler(void);
void psplinkDebugExHandler(void);
int psplinkHandleException(struct PsplinkContext *regs);
int psplinkRegisterExceptions(void *def, void *debug, void *ctx);

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

	return psplinkRegisterExceptions((void *) psplinkDefaultExHandler,
			(void *) psplinkDebugExHandler, g_psplinkContext);
}

/**
  * The entry point for our exception "trap"
  */
void psplinkTrap(struct PsplinkContext *ctx)
{
	int handled = 0;
	SceUID thid;

	thid = sceKernelGetThreadId();

	if((ctx == NULL) && (thid >= 0))
	{
		printf("No more free contexts for exception trap, deleting thread 0x%08X\n", thid);
		sceKernelExitDeleteThread(0);
	}

	if(thid < 0)
	{
		/* Means we are most likely in an interrupt context, just return */
		return;
	}

	ctx->thid = thid;
	if(ctx->regs.type == PSPLINK_EXTYPE_DEBUG)
	{
		struct DebugEnv env;

		if(!debugGetEnv(&env))
		{
			ctx->drcntl = env.DRCNTL;
		}
	}

	if(g_gdbhandler)
	{
		handled = g_gdbhandler(ctx);
	}

	if(handled == 0)
	{
		psplinkHandleException(ctx);
	}

	psplinkResumeFromException(ctx);
}
