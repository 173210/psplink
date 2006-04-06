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
#include <pspkdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "exception.h"
#include "apihook.h"
#include "tty.h"
#include "disasm.h"
#include "symbols.h"
#include "libs.h"

PSP_MODULE_INFO("PSPLINK", 0x1000, 1, 1);

#define WELCOME_MESSAGE "PSPLINK Initialised\n"

#define BOOTLOADER_NAME "PSPLINKLOADER"

struct GlobalContext g_context;

/* The thread ID of the loader */
static int g_loaderthid = 0;

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

SceUID load_wifi(const char *bootpath, int ap)
{
	char prx_path[MAXPATHLEN];
	char num[32];
	char *args[2];

	load_start_module("flash0:/kd/ifhandle.prx", 0, NULL);
	load_start_module("flash0:/kd/pspnet.prx", 0, NULL);
	load_start_module("flash0:/kd/pspnet_inet.prx", 0, NULL);
	load_start_module("flash0:/kd/pspnet_apctl.prx", 0, NULL);
	load_start_module("flash0:/kd/pspnet_resolver.prx", 0, NULL);

	sprintf(num, "%d", ap);
	args[0] = num;
	args[1] = NULL;
	strcpy(prx_path, bootpath);
	strcat(prx_path, "modnet.prx");
	g_context.wifi = ap;
	return load_start_module(prx_path, 1, args);
}

SceUID load_wifishell(const char *bootpath)
{
	char prx_path[MAXPATHLEN];

	strcpy(prx_path, bootpath);
	strcat(prx_path, "netshell.prx");
	g_context.wifishell = 1;
	return load_start_module(prx_path, 0, NULL);
}

SceUID load_usbshell(const char *bootpath)
{
	char prx_path[MAXPATHLEN];

	strcpy(prx_path, bootpath);
	strcat(prx_path, "usbshell.prx");
	g_context.usbshell = 1;
	return load_start_module(prx_path, 0, NULL);
}

SceUID load_conshell(const char *bootpath)
{
	char prx_path[MAXPATHLEN];

	strcpy(prx_path, bootpath);
	strcat(prx_path, "conshell.prx");
	g_context.conshell = 1;
	return load_start_module(prx_path, 0, NULL);
}

void copy_conscontext(const struct ConfigContext *cctx, struct GlobalContext *gctx)
{
	strcpy(gctx->conscrosscmd, cctx->conscrosscmd);
	strcpy(gctx->conssquarecmd, cctx->conssquarecmd);
	strcpy(gctx->constrianglecmd, cctx->constrianglecmd);
	strcpy(gctx->conscirclecmd, cctx->conscirclecmd);
	strcpy(gctx->consselectcmd, cctx->consselectcmd);
	strcpy(gctx->consstartcmd, cctx->consstartcmd);
	strcpy(gctx->consdowncmd, cctx->consdowncmd);
	strcpy(gctx->consleftcmd, cctx->consleftcmd);
	strcpy(gctx->consupcmd, cctx->consupcmd);
	strcpy(gctx->consrightcmd, cctx->consrightcmd);
}

SceUID load_gdb(const char *bootpath, int argc, char **argv)
{
	char prx_path[MAXPATHLEN];

	strcpy(prx_path, bootpath);
	if(g_context.usbgdb)
	{
		strcat(prx_path, "usbgdb.prx");
	}
	else
	{
		strcat(prx_path, "netgdb.prx");
	}
	g_context.gdb = 1;
	return load_start_module(prx_path, argc, argv);
}

void exit_reset(void)
{
	if(g_context.resetonexit)
	{
		psplinkReset();
	}
	else
	{
		psplinkSetK1(0);
		printf("\nsceKernelExitGame caught!\n");
		/* Kill the thread, bad idea to drop back to the program */
		sceKernelExitThread(0);
	}
}

void psplinkReset(void)
{
	struct SceKernelLoadExecParam le;
	int status;

	psplinkSetK1(0);
	printf("Resetting psplink\n");
	stop_usbmass();
	if(g_context.netshelluid >= 0)
	{
		sceKernelStopModule(g_context.netshelluid, 0, NULL, &status, NULL);
	}
	if(g_context.conshelluid >= 0)
	{
		sceKernelStopModule(g_context.conshelluid, 0, NULL, &status, NULL);
	}

	le.size = sizeof(le);
	le.args = strlen(g_context.bootfile) + 1;
	le.argp = (char *) g_context.bootfile;
	le.key = NULL;

	sceKernelLoadExec(g_context.bootfile, &le);
}

void psplinkExitShell(void)
{
	sceKernelExitGame();
}

int psplinkPresent(void)
{
	return 1;
}

int psplinkConsolePermit(void)
{
	return (!g_context.inexec || g_context.consinterfere);
}

int RegisterExceptionDummy(void)
{
	return 0;
}

/* Patch out the exception handler setup call for apps which come after us ;P */
int psplinkPatchException(void)
{
	u32 *addr;
	int intc;

	intc = pspSdkDisableInterrupts();
	addr = libsFindExportAddrByNid(refer_module_by_name("sceExceptionManager", NULL), "ExceptionManagerForKernel", 0x565C0B0E);
	if(addr)
	{
		*addr = (u32) RegisterExceptionDummy;
		sceKernelDcacheWritebackInvalidateRange(addr, 4);
		sceKernelIcacheInvalidateRange(addr, 4);
	}
	pspSdkEnableInterrupts(intc);

	return 0;
}

void initialise(SceSize args, void *argp)
{
	struct ConfigContext ctx;
	const char *init_dir = "ms0:/";

	map_firmwarerev();
	memset(&g_context, 0, sizeof(g_context));
	exceptionInit();
	g_context.netshelluid = -1;
	g_context.conshelluid = -1;
	parse_sceargs(args, argp);
	configLoad(g_context.bootpath, &ctx);
	disasmSetSymResolver(symbolFindNameByAddressEx);
	g_context.usbgdb = ctx.usbgdb;
	ttyInit();
	if(ctx.usbhost)
	{
		init_usbhost(g_context.bootpath);
		init_dir = "host0:/";
	}
	else if(ctx.usbmass)
	{
		init_usbmass();
	}

	if(shellInit(ctx.cliprompt, ctx.path, init_dir) < 0)
	{
		sceKernelExitGame();
	}

	if(ctx.baudrate == 0)
	{
		ctx.baudrate = DEFAULT_BAUDRATE;
	}

	if(ctx.sioshell)
	{
		sioInit(ctx.baudrate, 0);
		ttySetSioHandler(sioPutText);
		g_context.sioshell = 1;
	}
	else if(ctx.kprintf)
	{
		sioInit(ctx.baudrate, 1);
	}

	if(ctx.usbshell)
	{
		load_usbshell(g_context.bootpath);
	}

	sceUmdActivate(1, "disc0:");

	/* Hook sceKernelExitGame */
	apiHookByNid(refer_module_by_name("sceLoadExec", NULL), "LoadExecForUser", 0x05572A5F, exit_reset);

	sceKernelWaitThreadEnd(g_loaderthid, NULL);
	unload_loader();

	psplinkPatchException();

	if(ctx.enableuser)
	{
		load_psplink_user(g_context.bootpath);
	}

	if(ctx.wifi > 0)
	{
		load_wifi(g_context.bootpath, ctx.wifi);

		if(ctx.wifishell)
		{
			g_context.netshelluid = load_wifishell(g_context.bootpath);
		}
	}

	if (ctx.conshell)
	{
		g_context.consinterfere = ctx.consinterfere;
		g_context.conshelluid = load_conshell(g_context.bootpath);
		copy_conscontext(&ctx, &g_context);
	}

	g_context.resetonexit = ctx.resetonexit;
	g_context.pcterm  = ctx.pcterm;

	if(g_context.execfile[0] != 0)
	{
		if(load_start_module(g_context.execfile, g_context.execargc, g_context.execargv) >= 0)
		{
			g_context.inexec = 1;
		}
	}
}

/* Simple thread */
int main_thread(SceSize args, void *argp)
{
	initialise(args, argp);

	printf(WELCOME_MESSAGE);

	if(g_context.sioshell)
	{
		shellStart();

		if(g_context.netshelluid >= 0)
		{
			int status;
			sceKernelStopModule(g_context.netshelluid, 0, NULL, &status, NULL);
		}

		psplinkExitShell();
	}
	else
	{
		sceKernelSleepThread();
	}

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("PspLink", main_thread, 8, 16*1024, 0, NULL);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, args, argp);
	}

	return 0;
}
