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
#include "gdb-common.h"
#include "../psplink_user/psplink_user.h"

PSP_MODULE_INFO(GDB_MODULE_NAME, 0, 1, 1);
PSP_MAIN_THREAD_NAME("GDBServer");

/* Simple thread */
int main(int argc, char **argv)
{
	printf("PSPLink GDBServer (c) 2k6 TyRaNiD/Lovely2\n");
	memset(&g_context, 0, sizeof(g_context));
	g_context.evid = -1;
	g_context.mbx = -1;
	g_context.main_thread = sceKernelGetThreadId();
	GdbStubInit();

	if(argc < 2)
	{
		printf("usage: %s program.elf [args]\n", argv[0]);
		return 1;
	}

	g_context.evid = sceKernelCreateEventFlag("GdbEvent", PSP_EVENT_WAITMULTIPLE, 0, NULL);
	if(g_context.evid < 0)
	{
		printf(GDB_MODULE_NAME ": Error creating event flag 0x%08X\n", g_context.evid);
		return 1;
	}

	g_context.mbx = sceKernelCreateMbx("GdbExMbx", 0, 0);
	if(g_context.mbx < 0)
	{
		printf("Error, couldn't create message box 0x%08X\n", g_context.mbx);
		return 1;
	}

	if(isInit())
	{
		char *ext;

		ext = strrchr(argv[1], '.');
		if(ext)
		{
			if(strcasecmp(ext, ".elf") == 0)
			{
				g_context.elf = 1;
			}
		}

		g_context.uid = sceKernelLoadModule(argv[1], 0, NULL);
		if(g_context.uid < 0)
		{
			printf(GDB_MODULE_NAME ": Could not load %s - 0x%08X\n", argv[1], g_context.uid);
			return 1;
		}

		printf(GDB_MODULE_NAME ": Loaded %s - UID 0x%08X\n", argv[1], g_context.uid);

		g_context.info.size = sizeof(g_context.info);
		if(psplinkReferModule(g_context.uid, &g_context.info) == 0)
		{
			printf(GDB_MODULE_NAME ": Could not get module information\n");
			return 1;
		}

		/* Create a fake register block */
		g_context.regs.epc = g_context.info.entry_addr;
		g_context.regs.cause = 9 << 2;
		g_context.argc = argc - 1;
		g_context.argv = &argv[1];

		psplinkUserRegisterGdbHandler(GdbTrapEntry);
		start_server();
	}

	g_context.main_thread = -1;

	return 0;
}

int module_stop(SceSize args, void *argp)
{
	psplinkUserRegisterGdbHandler(NULL);

	if(g_context.main_thread >= 0)
	{
		sceKernelTerminateDeleteThread(g_context.main_thread);
		g_context.main_thread = -1;
	}

	if(g_context.evid >= 0)
	{
		sceKernelDeleteEventFlag(g_context.evid);
	}

	if(g_context.mbx >= 0)
	{
		sceKernelDeleteMbx(g_context.mbx);
	}

	stop_server();

	return 0;
}
