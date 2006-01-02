/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK bootstrap
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <string.h>
#include <stdio.h>
#include "../psplink/version.h"

PSP_MODULE_INFO("PSPLINKLOADER", 0x1000, 1, 1);
/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(0);

/* Define printf, just to make typing easier */
#define printf	pspDebugScreenPrintf

#define MAX_ARGS 16

char *g_argv[MAX_ARGS];
int  g_argc = 0;

SceUID load_module(const char *path, int flags, int type)
{
	SceKernelLMOption option;
	SceUID mpid;

	/* If the type is 0, then load the module in the kernel partition, otherwise load it
	   in the user partition. */
	if (type == 0) {
		mpid = 1;
	} else {
		mpid = 2;
	}

	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.mpidtext = mpid;
	option.mpiddata = mpid;
	option.position = 0;
	option.access = 1;

	return sceKernelLoadModule(path, flags, type > 0 ? &option : NULL);
}

void parse_args(SceSize args, void *argp)
{
	int  loc = 0;
	char *ptr = argp;

	while(loc < args)
	{
		g_argv[g_argc] = &ptr[loc];
		loc += strlen(&ptr[loc]) + 1;
		g_argc++;
		if(g_argc == (MAX_ARGS-1))
		{
			break;
		}
	}
	g_argv[g_argc] = NULL;
}

int build_args(char *args, const char *bootfile, SceUID thid, const char *execfile, int argc, char **argv)
{
	int loc = 0;
	int i;

	strcpy(args, bootfile);
	loc += strlen(bootfile) + 1;
	sprintf(&args[loc], "%08X", thid);
	loc += strlen(&args[loc]) + 1;
	if(execfile != NULL)
	{
		strcpy(&args[loc], execfile);
		loc += strlen(execfile) + 1;
		for(i = 0; i < argc; i++)
		{
			strcpy(&args[loc], argv[i]);
			loc += strlen(argv[i]) + 1;
		}
	}

	return loc;
}

int main_thread(SceSize args, void *argp)
{
	char prx_args[512];
	char prx_path[256];
	char *path;
	SceUID modid;
	int ret;

	pspDebugScreenInit();
	sceDisplayWaitVblankStart();

	pspSdkInstallNoDeviceCheckPatch();
	pspSdkInstallNoPlainModuleCheckPatch();

	parse_args(args, argp);
	path = strrchr(g_argv[0], '/');
	if(path != NULL)
	{
		memcpy(prx_path, g_argv[0], path - g_argv[0] + 1);
		prx_path[path - g_argv[0] + 1] = 0;
		strcat(prx_path, "psplink.prx");
	}
	else
	{
		/* Well try for a default */
		strcpy(prx_path, "ms0:/psplink.prx");
	}

	/* Start mymodule.prx and dump its information */
	printf("PSPLink Bootstrap TyRaNiD (c) 2k5 Version %s\n", PSPLINK_VERSION);
	modid = load_module(prx_path, 0, 0);
	if(modid >= 0)
	{
		int size;
		int status;

		printf("Starting psplink module\n");
		size = build_args(prx_args, g_argv[0], sceKernelGetThreadId(), g_argv[1], g_argc-2, &g_argv[2]);

		ret = sceKernelStartModule(modid, size, prx_args, &status, NULL);
		printf("Done\n");
	}
	else
	{
		printf("Error loading psplink module %08X\n", modid);
	}

	/* Let's bug out */
	sceKernelExitDeleteThread(0);

	return 0;
}

int module_start(SceSize args, void *argp) __attribute__((alias("_start")));

/* Entry point */
int _start(SceSize args, void *argp)
{
	int thid;
	u32 func;

	func = (u32) main_thread;
	func |= 0x80000000;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("main_thread", (void *) func, 0x20, 0x10000, 0, NULL);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, args, argp);
	}

	return 0;
}

int module_stop(void)
{
	return 0;
}
