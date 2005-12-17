/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK kernel module main code.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
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
#include <pspsysmem_kernel.h>
#include "memoryUID.h"
#include "psplink.h"
#include "psplinkcnf.h"
#include "parse_args.h"
#include "util.h"
#include "sio.h"
#include "shell.h"
#include "config.h"

PSP_MODULE_INFO("PSPLINK", 0x1000, 1, 1);

#define WELCOME_MESSAGE "PSPLINK Initialised\n"

#define BOOTLOADER_NAME "PSPLINKLOADER"

struct GlobalContext g_context;

/* The thread ID of the loader */
static int g_loaderthid = 0;
/* The two instruction pre-amble from the original exitgame function */
static u32 g_exitgame[2];

void save_execargs(int argc, char **argv);

int unload_loader(void)
{
	SceModule *mod;
	SceUID modid;
	int ret = 0;
	int status;

	mod = sceKernelFindModuleByName(BOOTLOADER_NAME);
	if(mod != NULL)
	{
		DEBUG_PRINTF("Loader UID: %08X\n", mod->modid);
		/* Stop module */
		modid = mod->modid;
		ret = sceKernelStopModule(modid, 0, NULL, &status, NULL);
		if(ret >= 0)
		{
			ret = sceKernelUnloadModule(modid);
		}
	}
	else
	{
		printf("Couldn't find bootloader\n");
	}

	return 0;
}

void parse_sceargs(SceSize args, void *argp)
{
	int  loc = 0;
	char *ptr = argp;
	int argc = 0;
	char *argv[MAX_ARGS];

	while(loc < args)
	{
		argv[argc] = &ptr[loc];
		loc += strlen(&ptr[loc]) + 1;
		argc++;
		if(argc == (MAX_ARGS-1))
		{
			break;
		}
	}

	argv[argc] = NULL;
	g_loaderthid = 0;

	if(argc > 0)
	{
		char *lastdir;

		g_context.bootfile = argv[0];
		lastdir = strrchr(argv[0], '/');
		if(lastdir != NULL)
		{
			memcpy(g_context.bootpath, argv[0], lastdir - argv[0] + 1);
		}
	}

	if(argc > 1)
	{
		char *endp;
		g_loaderthid = strtoul(argv[1], &endp, 16);
	}

	if(argc > 2)
	{
		strcpy(g_context.execfile, argv[2]);
		save_execargs(argc - 3, &argv[3]);
	}
}

void load_psplink_user(const char *bootpath)
{
	char prx_path[MAXPATHLEN];

	strcpy(prx_path, bootpath);
	strcat(prx_path, "psplink_user.prx");
	load_start_module(prx_path, 0, NULL);
}

void exit_reset(void)
{
	printf("\nsceKernelExitGame caught!\n");
}

/* Do some kernel patching */
void patch_kernel(void)
{
	u32 *jump;
	u32 *patch;

	jump = (u32 *) sceKernelExitGame;
	patch = (u32 *) (0x80000000 | ((*jump & 0x03FFFFFF) << 2));

	g_exitgame[0] = patch[0];
	g_exitgame[1] = patch[1];

	/* Patch in a jump to the reset function */
	patch[0] = 0x08000000 | ((((u32) exit_reset) & 0x0FFFFFFF) >> 2);
	patch[1] = 0;
	sceKernelDcacheWritebackAll();
	sceKernelIcacheInvalidateAll();
}

/* Do some kernel patching */
void unpatch_kernel(void)
{
	u32 *jump;
	u32 *patch;

	jump = (u32 *) sceKernelExitGame;
	patch = (u32 *) (0x80000000 | ((*jump & 0x03FFFFFF) << 2));

	patch[0] = g_exitgame[0];
	patch[1] = g_exitgame[1];

	sceKernelDcacheWritebackAll();
	sceKernelIcacheInvalidateAll();
}

void psplinkReset(void)
{
	struct SceKernelLoadExecParam le;

	stop_usb();

	le.size = sizeof(le);
	le.args = strlen(g_context.bootfile) + 1;
	le.argp = (char *) g_context.bootfile;
	le.key = NULL;

	sceKernelLoadExec(g_context.bootfile, &le);
}


/* Simple thread */
int main_thread(SceSize args, void *argp)
{
	struct ConfigContext ctx;

	DEBUG_START;
	DEBUG_PRINTF("Starting PSPLINK kernel module\n");
	map_firmwarerev();
	memset(&g_context, 0, sizeof(g_context));
	sioInit();
	sceUmdActivate(1, "disc0:");
	parse_sceargs(args, argp);
	DEBUG_PRINTF("Bootfile %s threadid %08X execfile %s\n", g_context.bootfile, g_loaderthid,
			g_context.execfile[0] == 0 ? "NULL" : g_context.execfile);
	patch_kernel();

	sceKernelWaitThreadEnd(g_loaderthid, NULL);
	unload_loader();

	configLoad(g_context.bootpath, &ctx);
	if(ctx.enableuser)
	{
		load_psplink_user(g_context.bootpath);
	}

	if(g_context.execfile[0] != 0)
	{
		if(load_start_module(g_context.execfile, g_context.execargc, g_context.execargv) >= 0)
		{
			g_context.inexec = 1;
		}
	}

	printf(WELCOME_MESSAGE);

	shellStart();

	unpatch_kernel();
	sceKernelExitGame();

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("PspLink", main_thread, 0x10, 0x10000, 0, NULL);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, args, argp);
	}

	return 0;
}
