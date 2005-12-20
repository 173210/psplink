/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * shell.c - PSPLINK kernel module shell code
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
#include <pspdisplay.h>
#include "memoryUID.h"
#include "psplink.h"
#include "psplinkcnf.h"
#include "parse_args.h"
#include "util.h"
#include "sio.h"
#include "bitmap.h"
#include "shell.h"
#include "script.h"

#define MAX_SHELL_VAR      128
#define SHELL_PROMPT	"psplink %d>"
/* Maximum command line */
#define CLI_MAX			128
/* Maximum history */
#define CLI_HISTSIZE	8

extern struct GlobalContext g_context;

int (*g_readchar)(void) = sioReadChar;
int (*g_readcharwithtimeout)(void) = sioReadCharWithTimeout;

/* Last command line (history) */
static char g_lastcli[CLI_HISTSIZE][CLI_MAX];
/* Current command line */
static char g_cli[CLI_MAX];
/* Current position in the command line buffer */
static int  g_cli_pos = 0;
/* Current size of the cli buffer */
static int  g_cli_size = 0;
/* Last position in the history buffer */
static int  g_lastcli_pos = 0;
/* Current scrolling position in the history buffer */
static int  g_currcli_pos = 0;

typedef int (*threadmanprint_func)(SceUID uid, int verbose);

#define MEM_ATTRIB_READ	 (1 << 0)
#define MEM_ATTRIB_WRITE (1 << 1)
#define MEM_ATTRIB_EXEC	 (1 << 2)
#define MEM_ATTRIB_BYTE  (1 << 3)
#define MEM_ATTRIB_HALF  (1 << 4)
#define MEM_ATTRIB_WORD  (1 << 5)
#define MEM_ATTRIB_DBL   (1 << 6)
#define MEM_ATTRIB_ALL	 0xFFFFFFFF

struct mem_entry
{
	u32 addr;
	s32 size;
	u32 attrib;
	const char *desc;
};

static struct mem_entry g_memareas[] = 
{
	{ 0x08800000, (24 * 1024 * 1024), MEM_ATTRIB_ALL, "User memory" },
	{ 0x48800000, (24 * 1024 * 1024), MEM_ATTRIB_ALL, "User memory (uncached)" },
	{ 0x88000000, (32 * 1024 * 1024), MEM_ATTRIB_ALL, "Kernel memory" },
	{ 0xC8000000, (32 * 1024 * 1024), MEM_ATTRIB_ALL, "Kernel memory (uncached)" },
	{ 0x04000000, (2 * 1024 * 1024), MEM_ATTRIB_ALL, "VRAM" },
	{ 0x44000000, (2 * 1024 * 1024), MEM_ATTRIB_ALL, "VRAM (uncached)" },
	{ 0, 0, 0, NULL }
};

static int validate_memaddr(u32 addr, u32 attrib)
{
	const struct mem_entry *entry;
	int size_left = 0;

	entry = g_memareas;

	while(entry->size != 0)
	{
		if((addr >= entry->addr) && (addr < (entry->addr + (u32) entry->size)))
		{
			// Only pass through areas with valid attributes (e.g. write or execute)
			if((entry->attrib & attrib) == attrib)
			{
				size_left = entry->size - (int) (addr - entry->addr);
			}
			break;
		}

		entry++;
	}

	return size_left;
}

static int decode_memaddr(const char *straddr, u32 *memaddr)
{
	int ret = 0;
	int deref = 0;
	int i = 0;
	u32 addr = 0;
	char *endp;

	/* We are dereferencing */
	while(*straddr == '@')
	{
		/* Incremement the deref count */
		deref++;
		straddr++;
	}

	addr = strtoul(straddr, &endp, 16);
	if(*endp != 0)
	{
		printf("Could not decode memory address '%s'\n", straddr);
		ret = 0;
	}
	else
	{
		ret = 1;
	}

	/* Only deref if it was valid */
	i = 0;
	while((i < deref) && (ret))
	{
		addr = addr & ~3;
		if(validate_memaddr(addr, MEM_ATTRIB_READ | MEM_ATTRIB_WORD))
		{
			addr = _lw(addr);
		}
		else
		{
			printf("Invalid de-reference at depth %d (%08x)\n", i, addr);
			ret = 0;
		}
		i++;
	}

	if(ret)
	{
		*memaddr = addr;
	}


	return ret;
}

struct shell_variable
{
	const char *name;
	char data[MAX_SHELL_VAR];
};

struct shell_variable g_shellvars[] = 
{
	{ "prompt", SHELL_PROMPT },
	{ NULL, "" },
};

char *find_shell_var(const char *name)
{
	int i;

	i = 0;
	while(g_shellvars[i].name)
	{
		if(strcmp(name, g_shellvars[i].name) == 0)
		{
			return g_shellvars[i].data;
		}
		i++;
	}

	return NULL;
}

int set_shell_var(const char *name, const char *data)
{
	char *vardata;

	vardata = find_shell_var(name);
	if(vardata == NULL)
	{
		return 0;
	}

	strncpy(vardata, data, MAX_SHELL_VAR-1);
	vardata[MAX_SHELL_VAR-1] = 0;

	return 1;
}

void print_prompt(void)
{
	char tmp[MAX_SHELL_VAR];
	const char *cliprompt;
	int in, out;

	cliprompt = find_shell_var("prompt");
	if(cliprompt == NULL)
	{
		printf("ERROR> ");
		return;
	}

	out = 0;
	in = 0;
	while((cliprompt[in]) && (out < (MAX_SHELL_VAR-1)))
	{
		if(cliprompt[in] == '%')
		{
			switch(cliprompt[in+1])
			{
				case '%': tmp[out++] = '%';
						  in += 2;
						  break;
				case 'd': strncpy(&tmp[out], g_context.currdir, MAX_SHELL_VAR - out - 1);
						  tmp[MAX_SHELL_VAR-1] = 0;
						  while(tmp[out])
						  {
							  out++;
						  }
						  in += 2;
						  break;
				default : in++;
						  break;
			};
		}
		else
		{
			tmp[out++] = cliprompt[in++];
		}
	}

	tmp[out] = 0;
	printf("%s ", tmp);
}

static SceUID get_module_uid(const char *name)
{
	char *endp;
	SceUID uid = -1;

	if(name[0] == '@')
	{
		SceModule *pMod;

		pMod = sceKernelFindModuleByName(&name[1]);
		if(pMod == NULL)
		{
			printf("ERROR: Invalid name %s\n", name);
			return CMD_ERROR;
		}
		uid = pMod->modid;
	}
	else
	{
		uid = strtoul(name, &endp, 16);
		if(*endp != 0)
		{
			printf("ERROR: Invalid uid %s\n", name);
			uid = -1;
		}
	}

	return uid;
}

typedef int (*ReferFunc)(const char *, SceUID *, void *);

static SceUID get_thread_uid(const char *name, ReferFunc pRefer)
{
	char *endp;
	SceUID uid = -1;

	if(name[0] == '@')
	{
		if(pRefer(&name[1], &uid, NULL) < 0)
		{
			printf("ERROR: Invalid name %s\n", name);
			return CMD_ERROR;
		}
	}
	else
	{
		uid = strtoul(name, &endp, 16);
		if(*endp != 0)
		{
			printf("ERROR: Invalid uid %s\n", name);
			uid = -1;
		}
	}

	return uid;
}

static int threadmanlist_cmd(int argc, char **argv, enum SceKernelIdListType type, const char *name, threadmanprint_func pinfo)
{
	SceUID ids[100];
	int ret;
	int count;
	int i;
	int verbose = 0;

	if(argc > 0)
	{
		if(strcmp(argv[0], "v"))
		{
			verbose = 1;
		}
	}

	memset(ids, 0, 100 * sizeof(SceUID));
	ret = sceKernelGetThreadmanIdList(type, ids, 100, &count);
	if(ret >= 0)
	{
		printf("<%s List>\n", name);
		for(i = 0; i < count; i++)
		{
			if(pinfo(ids[i], verbose) < 0)
			{
				printf("ERROR: Unknown %s %08X\n", name, ids[i]);
			}
		}
	}

	return CMD_OK;
}

static int threadmaninfo_cmd(int argc, char **argv, const char *name, threadmanprint_func pinfo, ReferFunc pRefer)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_thread_uid(argv[0], pRefer);

	if(uid >= 0)
	{
		if(pinfo(uid, 1) < 0)
		{
			printf("ERROR: Unknown %s %08X\n", name, uid);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int print_threadinfo(SceUID uid, int verbose)
{
	SceKernelThreadInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferThreadStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: %08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr: %08X - Status: %d - Entry: %p\n", info.attr, info.status, info.entry);
			printf("Stack: %p - StackSize %08X - GP: %08X\n", info.stack, info.stackSize,
					(u32) info.gpReg);
			printf("InitPri: %d - CurrPri: %d - WaitType %d\n", info.initPriority,
					info.currentPriority, info.waitType);
			printf("WaitId: %08X - WakeupCount: %d - ExitStatus: %08X\n", info.waitId,
					info.wakeupCount, info.exitStatus);
			printf("RunClocks: %d - IntrPrempt: %d - ThreadPrempt: %d\n", info.runClocks.low,
					info.intrPreemptCount, info.threadPreemptCount);
			printf("ReleaseCount: %d\n", info.releaseCount);
		}
	}

	return ret;
}

static int thlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Thread, "Thread", print_threadinfo);
}

static int thinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Thread", print_threadinfo, (ReferFunc) pspSdkReferThreadStatusByName);
}

static int thsusp_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferThreadStatusByName);

	if(uid >= 0)
	{
		err = sceKernelSuspendThread(uid);
		if(err < 0)
		{
			printf("Cannot suspend thread %08X\n", err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int thresm_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferThreadStatusByName);

	if(uid >= 0)
	{
		err = sceKernelResumeThread(uid);
		if(err < 0)
		{
			printf("Cannot suspend thread %08X\n", err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int thwake_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferThreadStatusByName);

	if(uid >= 0)
	{
		err = sceKernelWakeupThread(uid);
		if(err < 0)
		{
			printf("Cannot wakeup thread %08X\n", err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int thterm_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferThreadStatusByName);

	if(uid >= 0)
	{
		err = sceKernelTerminateThread(uid);
		if(err < 0)
		{
			printf("Cannot terminate thread %08X\n", err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int thdel_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferThreadStatusByName);

	if(uid >= 0)
	{
		err = sceKernelDeleteThread(uid);
		if(err < 0)
		{
			printf("Cannot delete thread %08X\n", err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int thtdel_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferThreadStatusByName);

	if(uid >= 0)
	{
		err = sceKernelTerminateDeleteThread(uid);
		if(err < 0)
		{
			printf("Cannot terminate delete thread %08X\n", err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int print_eventinfo(SceUID uid, int verbose)
{
	SceKernelEventFlagInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferEventFlagStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: %08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr: %08X - initPattern %08X - currPatten %08X\n", info.attr, info.initPattern, 
					info.currentPattern);
			printf("NumWaitThreads: %08X\n", info.numWaitThreads);
		}
	}

	return ret;
}

static int evlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_EventFlag, "EventFlag", print_eventinfo);
}

static int evinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "EventFlag", print_eventinfo, (ReferFunc) pspSdkReferEventFlagStatusByName);
}

static int print_semainfo(SceUID uid, int verbose)
{
	SceKernelSemaInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferSemaStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: %08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr: %08X - initCount: %08X - currCount: %08X\n", info.attr, info.initCount, 
					info.currentCount);
			printf("maxCount: %08X - NumWaitThreads: %08X\n", info.maxCount, info.numWaitThreads);
		}
	}

	return ret;
}

static int smlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Semaphore, "Semaphore", print_semainfo);
}

static int sminfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Semaphore", print_semainfo, (ReferFunc) pspSdkReferSemaStatusByName);
}

static int uidlist_cmd(int argc, char **argv)
{
	printUIDList();

	return CMD_OK;
}

static int print_modinfo(SceUID uid, int verbose)
{
	SceKernelModuleInfo info;
	int ret;

	pspDebugSioDisableKprintf();
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	ret = g_QueryModuleInfo(uid, &info);
	if(ret >= 0)
	{
		printf("UID: %08X Attr: %04X - Name: %s\n", uid, info.attribute, info.name);
		if(verbose)
		{
			printf("Entry: %08X - GP: %08X - TextAddr: %08X\n", info.entry_addr,
					info.gp_value, info.text_addr);
			printf("TextSize: %08X - DataSize: %08X BssSize: %08X\n", info.text_size,
					info.data_size, info.bss_size);
		}
	}
	pspDebugSioEnableKprintf();

	return ret;
}

static int modinfo_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);

	if(uid >= 0)
	{
		if(print_modinfo(uid, 1) < 0)
		{
			printf("ERROR: Unknown module %08X\n", uid);
		}
		else
		{
			ret = CMD_OK;
		}
	}
	else
	{
		printf("ERROR: Invalid module %s\n", argv[0]);
	}

	return ret;
}

static int modlist_cmd(int argc, char **argv)
{
	SceUID ids[100];
	int ret;
	int count;
	int i;
	int verbose = 0;

	if(argc > 0)
	{
		if(strcmp(argv[0], "v") == 0)
		{
			verbose = 1;
		}
	}

	memset(ids, 0, 100 * sizeof(SceUID));
	ret = g_GetModuleIdList(ids, 100 * sizeof(SceUID), &count);
	if(ret >= 0)
	{
		printf("<Module List>\n");
		for(i = 0; i < count; i++)
		{
			print_modinfo(ids[i], verbose);
		}
	}

	return CMD_OK;
}

static int modstop_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		SceUID uid_ret;
		int status;

		uid_ret = sceKernelStopModule(uid, 0, NULL, &status, NULL);
		printf("Module Stop %08X Status %08X\n", uid_ret, status);

		ret = CMD_OK;
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modunld_cmd(int argc, char **argv)
{

	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		SceUID uid_ret;

		uid_ret = sceKernelUnloadModule(uid);
		printf("Module Unload %08X\n", uid_ret);

		ret = CMD_OK;
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;

}

static int modstart_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	char args[1024];
	int  len;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		SceUID uid_ret;
		int status;

		if(argc > 1)
		{
			len = build_args(args, argv[1], argc - 2, &argv[2]);
		}
		else
		{
			len = build_args(args, "unknown", 0, NULL);
		}

		uid_ret = sceKernelStartModule(uid, len, args, &status, NULL);
		printf("Module Start %08X Status %08X\n", uid_ret, status);

		ret = CMD_OK;
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modload_cmd(int argc, char **argv)
{
	SceUID modid;
	char path[1024];

	if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
	{
		modid = sceKernelLoadModule(path, 0, NULL);
		printf("Module Load '%s' UID: %08X\n", path, modid);
	}
	else
	{
		printf("Error invalid file %s\n", path);
	}

	return CMD_OK;
}

static int modexec_cmd(int argc, char **argv)
{
	char path[1024];
	char args[1024];
	int  len;
	struct SceKernelLoadExecParam le;

	if(argc > 0)
	{
		if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
		{
			len = build_args(args, argv[1], argc - 1, &argv[1]);
			le.size = sizeof(le);
			le.args = len;
			le.argp = args;
			le.key = NULL;

			sceKernelLoadExec(path, &le);
		}
	}

	return CMD_OK;
}

static int ldstart_cmd(int argc, char **argv)
{
	char path[1024];
	int ret = CMD_ERROR;

	if(argc > 0)
	{
		SceUID modid;

		if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
		{
			modid = load_start_module(path, argc-1, &argv[1]);
			if(modid >= 0)
			{
				printf("Load/Start module UID: %08X\n", modid);
			}
			else
			{
				printf("Failed to Load/Start module '%s' Error: %08X\n", path, modid);
			}

			ret = CMD_OK;
		}
		else
		{
			printf("Error invalid file %s\n", path);
		}
	}

	return ret;
}

static int reset_cmd(int argc, char **argv)
{
	psplinkReset();

	return CMD_OK;
}

static int exec_cmd(int argc, char **argv)
{
	struct SceKernelLoadExecParam le;
	char args[512];
	int size;
	int ret = CMD_ERROR;
	char file[1024];
	char *exe;

	do
	{
		if((g_context.inexec) && (argc == 0))
		{
			exe = g_context.execfile;
		}
		else
		{
			if(argc > 0)
			{
				exe = argv[0];
			}
			else
			{
				break;
			}
		}

		if(handlepath(g_context.currdir, exe, file, TYPE_FILE, 1) == 0)
		{
			printf("Error, invalid file %s\n", file);
			break;
		}

		printf("Exec '%s'\n", file);

		if(g_context.inexec)
		{
			if(argc == 0)
			{
				size = build_bootargs(args, g_context.bootfile, file, g_context.execargc, g_context.execargv);
			}
			else
			{
				size = build_bootargs(args, g_context.bootfile, file, argc-1, &argv[1]);
			}

			stop_usb();

			le.size = sizeof(le);
			le.args = size;
			le.argp = (char *) args;
			le.key = NULL;

			sceKernelLoadExec(g_context.bootfile, &le);
		}
		else
		{
			SceUID modid;

			modid = load_start_module(file, argc-1, &argv[1]);
			if(modid >= 0)
			{
				printf("Load/Start module UID: %08X\n", modid);
				strcpy(g_context.execfile, file);
				g_context.inexec = 1;
				save_execargs(argc-1, &argv[1]);
				ret = CMD_OK;
			}
			else
			{
				printf("Failed to Load/Start module '%s' Error: %08X\n", file, modid);
			}
		}
	}
	while(0);

	return ret;
}

static int debug_cmd(int argc, char **argv)
{
	char *file;

	do
	{
		file = argv[0];

		if(g_context.inexec)
		{
			printf("ERROR: Reset before going into debug mode\n");
		}
		else
		{
			SceUID modid;

			modid = load_start_module_debug(file);
			if(modid >= 0)
			{
				printf("Load/Start module UID: %08X\n", modid);
			}
			else
			{
				printf("Failed to Load/Start module '%s' Error: %08X\n", file, modid);
			}
		}
	}
	while(0);

	return CMD_OK;
}

static int list_dir(const char *name)
{
	int dfd;
	static SceIoDirent dir;

	dfd = sceIoDopen(name);
	if(dfd >= 0)
	{
		memset(&dir, 0, sizeof(dir));
		while(sceIoDread(dfd, &dir) > 0)
		{
			int ploop;

			if(dir.d_stat.st_attr & FIO_SO_IFDIR)
			{
				printf("d");
			}
			else
			{
				printf("-");
			}

			for(ploop = 2; ploop >= 0; ploop--)
			{
				int bits;

				bits = (dir.d_stat.st_mode >> (ploop * 3)) & 0x7;
				if(bits & 4)
				{
					printf("r");
				}
				else
				{
					printf("-");
				}

				if(bits & 2)
				{
					printf("w");
				}
				else
				{
					printf("-");
				}

				if(bits & 1)
				{
					printf("x");
				}
				else
				{
					printf("-");
				}
			}

			printf(" %8d ", (int) dir.d_stat.st_size);
			printf("%02d-%02d-%04d %02d:%02d ", dir.d_stat.st_mtime.day, 
					dir.d_stat.st_mtime.month, dir.d_stat.st_mtime.year,
					dir.d_stat.st_mtime.hour, dir.d_stat.st_mtime.minute);
			printf("%s\n", dir.d_name);
			memset(&dir, 0, sizeof(dir));
		}

		sceIoDclose(dfd);
	}
	else
	{
		printf("Could not open directory '%s'\n", name);
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int ls_cmd(int argc, char **argv)
{
	char path[1024];

	if(argc == 0)
	{
		printf("Listing directory %s\n", g_context.currdir);
		list_dir(g_context.currdir);
	}
	else
	{
		int loop;
		/* Strip whitespace and append a final slash */

		for(loop = 0; loop < argc; loop++)
		{
			if(handlepath(g_context.currdir, argv[loop], path, TYPE_DIR, 1))
			{
				printf("Listing directory %s\n", path);
				list_dir(path);
			}
		}
	}

	return CMD_OK;
}

static int chdir_cmd(int argc, char **argv)
{
	char *dir;
	int ret = CMD_ERROR;
	char path[1024];

	/* Get remainder of string */
	dir = argv[0];
	/* Strip whitespace and append a final slash */

	if(handlepath(g_context.currdir, dir, path, TYPE_DIR, 1) == 0)
	{
		printf("'%s' not a valid directory\n", dir);
	}
	else
	{
		strcpy(g_context.currdir, path);
		ret = CMD_OK;
	}

	return ret;
}

static int pwd_cmd(int argc, char **argv)
{
	printf("%s\n", g_context.currdir);

	return CMD_OK;
}

static int usbon_cmd(int argc, char **argv)
{
	(void) init_usb();

	return CMD_OK;
}

static int usboff_cmd(int argc, char **argv)
{
	(void) stop_usb();

	return CMD_OK;
}

static int usbstat_cmd(int argc, char **argv)
{
	u32 state;

	state = sceUsbGetState();
	printf("USB Status:\n");
	printf("Connection    : %s\n", state & PSP_USB_ACTIVATED ? "activated" : "deactivated");
	printf("USB Cable     : %s\n", state & PSP_USB_CABLE_CONNECTED ? "connected" : "disconnected");
	printf("USB Connection: %s\n", state & PSP_USB_ACTIVATED ? "established" : "notpresent");

	return CMD_OK;
}

static int rename_cmd(int argc, char **argv)
{
	char asrc[MAXPATHLEN], adst[MAXPATHLEN];
	char *src, *dst;

	src = argv[0];
	dst = argv[1];

	if( !handlepath(g_context.currdir, src, asrc, TYPE_FILE, 1) )
		return CMD_ERROR;

	if( !handlepath(g_context.currdir, dst, adst, TYPE_FILE, 0) )
		return CMD_ERROR;

	if( sceIoRename(asrc, adst) < 0)
		return CMD_ERROR;

	printf("rename %s -> %s\n", asrc, adst);

	return CMD_OK;
}

static int rm_cmd(int argc, char **argv)
{
	char *file, afile[MAXPATHLEN];

	file = argv[0];

	if( !handlepath(g_context.currdir, file, afile, TYPE_FILE, 1) )
		return CMD_ERROR;

	if( sceIoRemove(afile) < 0 )
		return CMD_ERROR;

	printf("rm %s\n", afile);

	return CMD_OK;
}

static int mkdir_cmd(int argc, char **argv)
{
	char *file, afile[MAXPATHLEN];

	file = argv[0];

	if( !handlepath(g_context.currdir, file, afile, TYPE_FILE, 0) )
		return CMD_ERROR;

	if( sceIoMkdir(afile, 0777) < 0 )
		return CMD_ERROR;

	printf("mkdir %s\n", afile);

	return CMD_OK;
}

static int rmdir_cmd(int argc, char **argv)
{
	char *file, afile[MAXPATHLEN];

	file = argv[0];

	if( !handlepath(g_context.currdir, file, afile, TYPE_FILE, 0) )
		return CMD_ERROR;

	if( sceIoRmdir(afile) < 0 )
		return CMD_ERROR;

	printf("rmdir %s\n", afile);

	return CMD_OK;
}

static int cp_cmd(int argc, char **argv)
{
	int in, out;
	int n;
	char *source;
	char *destination;

	char fsrc[MAXPATHLEN];
	char fdst[MAXPATHLEN];
	char buff[2048];

	source = argv[0];
	destination = argv[1];

	if( !handlepath(g_context.currdir, source, fsrc, TYPE_FILE, 1) )
		return CMD_ERROR;
	
	if( !handlepath(g_context.currdir, destination, fdst, TYPE_ETHER, 0) )
		return CMD_ERROR;

	printf("cp %s -> %s\n", fsrc, fdst);

	in = sceIoOpen(fsrc, PSP_O_RDONLY, 0777);
	out = sceIoOpen(fdst, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if(in < 0 || out < 0)
		return CMD_ERROR;

	while(1) {
		n = sceIoRead(in, buff, 2048);

		if(n == 0)
			break;
		
		write(out, buff, n);
	}
	
	sceIoClose(in);
	sceIoClose(out);

	return CMD_OK;
}

static int meminfo_cmd(int argc, char **argv)
{
	int i;
	int pid = 1;
	int max = 5;

	if(argc > 0)
	{
		pid = atoi(argv[0]);
		printf("pid: %d\n", pid);
		if((pid <= 0) || (pid > 4))
		{
			printf("Error, invalid partition number %d\n", pid);
			return CMD_ERROR;
		}
		max = pid + 1;
	}

	printf("Memory Partitions:\n");
	printf("N |   BASE   |   SIZE   | TOTALFREE |  MAXFREE  | ATTR |\n");
	printf("--|----------|----------|-----------|-----------|------|\n");
	for(i = pid; i < max; i++)
	{
		SceSize total;
		SceSize free;
		PspSysmemPartitionInfo info;

		free = sceKernelPartitionMaxFreeMemSize(i);
		total = sceKernelPartitionTotalFreeMemSize(i);
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		sceKernelQueryMemoryPartitionInfo(i, &info);
		printf("%d | %08X | %8d | %9d | %9d | %04X |\n", 
				i, info.startaddr, info.memsize, total, free, info.attr);
	}

	return CMD_OK;
}

static int memreg_cmd(int argc, char **argv)
{
	int i;
	printf("Memory Regions:\n");
	i = 0;
	while(g_memareas[i].addr)
	{
		printf("Region %d: Base %08X - Size %08X - %s\n", i,
				g_memareas[i].addr, g_memareas[i].size, g_memareas[i].desc);
		i++;
	}

	return CMD_OK;
}

/* Maximum memory dump size (per screen) */
#define MAX_MEMDUMP_SIZE 256

/* Print a row of a memory dump, up to row_size */
static void print_row(const u32* row, s32 row_size, u32 addr)
{
	int i = 0;

	printf("%08x - ", addr);
	for(i = 0; i < 16; i++)
	{
		if(i < row_size)
		{
			printf("%02x ", row[i]);
		}
		else
		{
			printf("-- ");
		}
	}

	printf("- ");
	for(i = 0; i < 16; i++)
	{
		if(i < row_size)
		{
			if((row[i] >= 32) && (row[i] < 127))
			{
				printf("%c", row[i]);
			}
			else
			{
				printf(".");
			}
		}
		else
		{
			printf(".");
		}
	}

	printf("\n");
}


/* Print a memory dump to SIO */
static void print_memdump(u32 addr, s32 size)
{
	int size_left;
	u32 row[16];
	int row_size;
	u8 *p_addr = (u8 *) addr;

	printf("         - 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f - 0123456789abcdef\n");
	printf("-----------------------------------------------------------------------------\n");

	size_left = size > MAX_MEMDUMP_SIZE ? MAX_MEMDUMP_SIZE : size;
	row_size = 0;

	while(size_left > 0)
	{
		row[row_size] = p_addr[row_size];
		row_size++;
		if(row_size == 16)
		{
			// draw row
			print_row(row, row_size, (u32) p_addr);
			p_addr += 16;
			row_size = 0;
		}

		size_left--;
	}
}

static int memdump_cmd(int argc, char **argv)
{
	u32 addr;
	s32 size_left;

	/* Get memory address */
	if(decode_memaddr(argv[0], &addr))
	{
		size_left = validate_memaddr(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);

		if(size_left > 0)
		{
			while(size_left > 0)
			{
				char ch;

				print_memdump(addr, size_left);

				printf("Press b to go back, space to go forward, or q to quit.\n");
				while((ch = g_readchar()) == -1);

				ch = upcase(ch);
				if(ch == 'Q')
				{
					break;
				}

				if(ch == 'B')
				{
					addr -= MAX_MEMDUMP_SIZE;
				}
				else
				{
					addr += MAX_MEMDUMP_SIZE;
				}

				size_left = validate_memaddr(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
			}
		}
		else
		{
			printf("Invalid memory address %x\n", addr);
		}
	}
	else
	{
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int savemem_cmd(int argc, char **argv)
{
	u32 addr;
	int size;
	int written;
	char *endp;

	size = strtoul(argv[1], &endp, 0);
	if(*endp != 0)
	{
		printf("Size parameter invalid '%s'\n", argv[1]);
		return CMD_ERROR;
	}

	if(decode_memaddr(argv[0], &addr))
	{
		int size_left;
		int fd;

		size_left = validate_memaddr(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size > size_left ? size_left : size;
		fd = sceIoOpen(argv[2], PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
		if(fd < 0)
		{
			printf("Could not open file '%s' for writing %08X\n", argv[2], fd);
		}
		else
		{
			written = 0;
			while(written < size)
			{
				int ret;

				ret = sceIoWrite(fd, (void *) (addr + written), size - written);
				if(ret <= 0)
				{
					printf("Could not write out file\n");
					break;
				}

				written += ret;
			}
			sceIoClose(fd);
		}
	}
	else
	{
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int loadmem_cmd(int argc, char **argv)
{
	u32 addr;
	int maxsize = -1;
	char *endp;

	if(argc > 2)
	{
		maxsize = strtoul(argv[2], &endp, 0);
		if(*endp != 0)
		{
			printf("Size parameter invalid '%s'\n", argv[2]);
			return CMD_ERROR;
		}
	}

	if(decode_memaddr(argv[0], &addr))
	{
		int size_left;
		int readbytes;
		int fd;

		size_left = validate_memaddr(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		fd = sceIoOpen(argv[1], PSP_O_RDONLY, 0777);
		if(fd < 0)
		{
			printf("Could not open file '%s' for reading %08X\n", argv[2], fd);
		}
		else
		{
			int size = 0;

			if(maxsize >= 0)
			{
				size = maxsize > size_left ? size_left : maxsize;
			}
			else
			{
				size = size_left;
			}

			readbytes = 0;
			while(readbytes < size)
			{
				int ret;

				ret = sceIoRead(fd, (void *) (addr + readbytes), size - readbytes);
				if(ret < 0)
				{
					printf("Could not write out file\n");
					break;
				}
				else if(ret == 0)
				{
					break;
				}

				readbytes += ret;
			}
			sceIoClose(fd);
			printf("Read %d bytes into memory\n", readbytes);
		}
	}
	else
	{
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int pokew_cmd(int argc, char **argv)
{
	u32 addr;

	if(decode_memaddr(argv[0], &addr))
	{
		int size_left;
		int i;

		addr &= ~3;
		size_left = validate_memaddr(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_WORD);
		if(size_left > 0)
		{
			for(i = 1; i < argc; i++)
			{
				u32 data;
				char *endp;

				data = strtoul(argv[i], &endp, 0);
				if(*endp == 0)
				{
					_sw(data, addr);
				}
				else
				{
					printf("Invalid value %s\n", argv[i]);
				}

				addr += 4;
				size_left -= 4;
				if(size_left == 0)
				{
					break;
				}
			}
		}
		else
		{
			printf("Invalid memory address %08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int pokeh_cmd(int argc, char **argv)
{
	u32 addr;

	if(decode_memaddr(argv[0], &addr))
	{
		int size_left;
		int i;

		addr &= ~1;
		size_left = validate_memaddr(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_HALF);
		if(size_left > 0)
		{
			for(i = 1; i < argc; i++)
			{
				u32 data;
				char *endp;

				data = strtoul(argv[i], &endp, 0);
				if(*endp == 0)
				{
					_sh(data, addr);
				}
				else
				{
					printf("Invalid value %s\n", argv[i]);
				}

				addr += 2;
				size_left -= 2;
				if(size_left == 0)
				{
					break;
				}
			}
		}
		else
		{
			printf("Invalid memory address %08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int pokeb_cmd(int argc, char **argv)
{
	u32 addr;

	if(decode_memaddr(argv[0], &addr))
	{
		int size_left;
		int i;

		size_left = validate_memaddr(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
		if(size_left > 0)
		{
			for(i = 1; i < argc; i++)
			{
				u32 data;
				char *endp;

				data = strtoul(argv[i], &endp, 0);
				if(*endp == 0)
				{
					_sb(data, addr);
				}
				else
				{
					printf("Invalid value %s\n", argv[i]);
				}

				addr += 1;
				size_left -= 1;
				if(size_left == 0)
				{
					break;
				}
			}
		}
		else
		{
			printf("Invalid memory address %08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int disasm_cmd(int argc, char **argv)
{
	u32 addr;
	int count = 1;
	int i;

	if(argc > 1)
	{
		char *endp;
		count = strtoul(argv[1], &endp, 0);
		if(*endp != 0)
		{
			printf("Invalid count argument\n");
			return CMD_ERROR;
		}
	}

	if(decode_memaddr(argv[0], &addr))
	{
		int size_left;

		addr &= ~3;
		size_left = validate_memaddr(addr, MEM_ATTRIB_READ | MEM_ATTRIB_WORD | MEM_ATTRIB_EXEC);
		if((size_left / 4) < count)
		{
			count = size_left / 4;
		}

		for(i = 0; i < count; i++)
		{
			printf("%s\n", PSPdis(addr));
			addr += 4;
		}
	}

	return CMD_OK;
}

static int scrshot_cmd(int argc, char **argv)
{
	SceUID block_id;
	void *block_addr;
	void *frame_addr;
	int frame_width;
	int pixel_format;
	int sync = 1;

	block_id = sceKernelAllocPartitionMemory(4, "scrshot", PSP_SMEM_Low, 544*1024, NULL);
	if(block_id < 0)
	{
		printf("Error could not allocate memory buffer %08X\n", block_id);
		return CMD_ERROR;
	}
	 
	block_addr = sceKernelGetBlockHeadAddr(block_id);

	sceDisplayGetFrameBuf(&frame_addr, &frame_width, &pixel_format, &sync);
	printf("frame_addr %p, frame_width %d, pixel_format %d\n", frame_addr, frame_width, pixel_format);

	if(frame_addr != NULL)
	{
		memcpy(block_addr, (void *) ((u32) frame_addr | 0x40000000), 544*1024);

		bitmapWrite(block_addr, pixel_format, argv[0]);
	}
	else
	{
		printf("Invalid frame address\n");
	}

	sceKernelFreePartitionMemory(block_id);

	return CMD_OK;
}

static int set_cmd(int argc, char **argv)
{
	if(argc == 0)
	{
		int i = 0;
		while(g_shellvars[i].name)
		{
			printf("%s=%s\n", g_shellvars[i].name, g_shellvars[i].data);
			i++;
		}
	}
	else
	{
		char *equals;

		equals = strchr(argv[0], '=');
		if(equals)
		{
			*equals = 0;
			equals++;
			if(set_shell_var(argv[0], equals) == 0)
			{
				printf("Error, couldn't find shell variable '%s'\n", argv[0]);
			}
		}
		else
		{
			printf("Error, must be of the form var=value\n");
		}
	}

	return CMD_OK;
}

static int run_cmd(int argc, char **argv)
{
	char path[1024];
	int ret = CMD_ERROR;

	if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
	{
		ret = scriptRun(path, argc, argv, 0);
	}
	else
	{
		printf("Invalid file %s\n", path);
	}

	return ret;
}

static int exit_cmd(int argc, char **argv)
{
	return CMD_EXITSHELL;
}

static int help_cmd(int argc, char **argv);

#define SHELL_TYPE_CMD  0
#define SHELL_TYPE_CATEGORY 1

/* Structure to hold a single command entry */
struct sh_command 
{
	const char *name;		/* Normal name of the command */
	const char *syn;		/* Synonym of the command */
	int (*func)(int argc, char **argv);		/* Pointer to the command function */
	int min_args;
	const char *desc;		/* Textual description */
	const char *help;		/* Command usage */
	int type;
};

/* Define the list of commands */
struct sh_command commands[] = {
	{ "thread", NULL, NULL, 0, "Commands to manipulate threads", NULL, SHELL_TYPE_CATEGORY },
	{ "thlist", "tl", thlist_cmd, 0, "List the threads in the system", "tl [v]", SHELL_TYPE_CMD },
	{ "thinfo", "ti", thinfo_cmd, 1, "Print info about a thread", "ti uid|@name" , SHELL_TYPE_CMD},
	{ "thsusp", "ts", thsusp_cmd, 1, "Suspend a thread", "ts uid|@name" , SHELL_TYPE_CMD},
	{ "thresm", "tr", thresm_cmd, 1, "Resume a thread", "tr uid|@name" , SHELL_TYPE_CMD},
	{ "thwake", "tw", thwake_cmd, 1, "Wakeup a thread", "tw uid|@name" , SHELL_TYPE_CMD},
	{ "thterm", "tt", thterm_cmd, 1, "Terminate a thread", "tt uid|@name" , SHELL_TYPE_CMD},
	{ "thdel", "td", thdel_cmd, 1, "Delete a thread", "td uid|@name" , SHELL_TYPE_CMD},
	{ "thtdel", "tx", thtdel_cmd, 1, "Terminate and delete a thread", "tx uid|@name" , SHELL_TYPE_CMD},
	{ "evlist", "el", evlist_cmd, 0, "List the event flags in the system", "el [v]", SHELL_TYPE_CMD },
	{ "evinfo", "ei", evinfo_cmd, 1, "Print info about an event flag", "ei uid|@name", SHELL_TYPE_CMD },
	{ "smlist", "sl", smlist_cmd, 0, "List the semaphores in the system", "sl [v]", SHELL_TYPE_CMD },
	{ "sminfo", "si", sminfo_cmd, 1, "Print info about a semaphore", "si uid|@name", SHELL_TYPE_CMD },
	
	{ "module", NULL, NULL, 0, "Commands to handle modules", NULL, SHELL_TYPE_CATEGORY },
	{ "modlist","ml", modlist_cmd, 0, "List the currently loaded modules", "ml [v]", SHELL_TYPE_CMD },
	{ "modinfo","mi", modinfo_cmd, 1, "Print info about a module", "mi uid|@name", SHELL_TYPE_CMD },
	{ "modstop","ms", modstop_cmd, 1, "Stop a running module", "ms uid|@name", SHELL_TYPE_CMD },
	{ "modunld","mu", modunld_cmd, 1, "Unload a module (must be stopped)", "mu uid|@name", SHELL_TYPE_CMD },
	{ "modload","md", modload_cmd, 1, "Load a module", "md path", SHELL_TYPE_CMD },
	{ "modstart","mt", modstart_cmd, 1, "Start a module", "mt uid|@name [args]", SHELL_TYPE_CMD },
	{ "modexec","me", modexec_cmd, 1, "LoadExec a module", "me path [args]", SHELL_TYPE_CMD },
	{ "exec", "e", exec_cmd, 0, "Execute a new program (under psplink)", "exec [path] [args]", SHELL_TYPE_CMD },
	{ "debug", "d", debug_cmd, 1, "Debug an executable (need to switch to gdb)", "debug path", SHELL_TYPE_CMD },
	{ "ldstart","ld", ldstart_cmd, 1, "Load and start a module", "ld path [args]", SHELL_TYPE_CMD },
	
	{ "memory", NULL, NULL, 0, "Commands to manipulate memory", NULL, SHELL_TYPE_CATEGORY },
	{ "meminfo", "mf", meminfo_cmd, 0, "Print free memory info", "mf [partitionid]", SHELL_TYPE_CMD },
	{ "memreg",  "mr", memreg_cmd, 0, "Print available memory regions (for other commands)", "mr", SHELL_TYPE_CMD },
	{ "memdump", "dm", memdump_cmd, 1, "Dump memory to screen", "md address", SHELL_TYPE_CMD },
	{ "savemem", "sm", savemem_cmd, 3, "Save memory to a file", "sm adresss size file", SHELL_TYPE_CMD },
	{ "loadmem", "lm", loadmem_cmd, 2, "Load memory from a file", "lm address file [maxsize]", SHELL_TYPE_CMD },
	{ "pokew",   "pw", pokew_cmd, 2, "Poke words into memory", "pw address val1 [val2..valN]", SHELL_TYPE_CMD },
	{ "pokeh",   "pw", pokeh_cmd, 2, "Poke half words into memory", "ph address val1 [val2..valN]", SHELL_TYPE_CMD },
	{ "pokeb",   "pw", pokeb_cmd, 2, "Poke bytes into memory", "pb address val1 [val2..valN]", SHELL_TYPE_CMD },
	{ "disasm",  "di", disasm_cmd, 1, "Disassemble instructions", "di address [count]", SHELL_TYPE_CMD },
	
	{ "fileio", NULL, NULL, 0, "Commands to handle file io", NULL, SHELL_TYPE_CATEGORY },
	{ "ls",  "dir", ls_cmd,    0, "List the files in a directory", "ls [path]", SHELL_TYPE_CMD },
	{ "chdir", "cd", chdir_cmd, 1, "Change the current directory", "cd path", SHELL_TYPE_CMD },
	{ "cp",  "copy", cp_cmd, 2, "Copy a file", "cp source destination", SHELL_TYPE_CMD },
	{ "mkdir", "md", mkdir_cmd, 1, "Make a Directory", "mkdir dir", SHELL_TYPE_CMD },
	{ "rm", "del", rm_cmd, 1, "Removes a File", "rm file", SHELL_TYPE_CMD },
	{ "rmdir", "rd", rmdir_cmd, 1, "Removes a Director", "rmdir dir", SHELL_TYPE_CMD },
	{ "rename", "ren", rename_cmd, 2, "Renames a File", "rename src dst", SHELL_TYPE_CMD },
	{ "pwd",   NULL, pwd_cmd, 0, "Print the current working directory", "pwd", SHELL_TYPE_CMD },

	{ "misc", NULL, NULL, 0, "Miscellaneous commands (e.g. USB, exit)", NULL, SHELL_TYPE_CATEGORY },
	{ "usbon", "un", usbon_cmd, 0, "Enable USB mass storage device", "usbon", SHELL_TYPE_CMD },
	{ "usboff", "uf", usboff_cmd, 0, "Disable USB mass storage device", "usboff", SHELL_TYPE_CMD },
	{ "usbstat", "us", usbstat_cmd, 0, "Display the status of the USB connection", "usbstat", SHELL_TYPE_CMD },
    { "uidlist","ul", uidlist_cmd, 0, "List the system UIDS", "ul", SHELL_TYPE_CMD },
	{ "exit", "quit", exit_cmd, 0, "Exit the shell", "exit", SHELL_TYPE_CMD },
	{ "set", NULL, set_cmd, 0, "Set a shell variable", "set [var=value]", SHELL_TYPE_CMD },
	{ "scrshot", "ss", scrshot_cmd, 1, "Take a screen shot", "ss file", SHELL_TYPE_CMD },
	{ "run",  NULL, run_cmd, 1, "Run a shell script", "run file [args]", SHELL_TYPE_CMD },
	{ "reset", "r", reset_cmd, 0, "Reset", "r", SHELL_TYPE_CMD },
	{ "help", "?", help_cmd, 0, "Help (Obviously)", "help [command]", SHELL_TYPE_CMD },
	{ NULL, NULL, NULL, 0, NULL, NULL, SHELL_TYPE_CMD }
};

/* Find a command from the command list */
static struct sh_command* find_command(const char *cmd)
{
	struct sh_command* found_cmd = NULL;
	int cmd_loop;

	for(cmd_loop = 0; commands[cmd_loop].name != NULL; cmd_loop++)
	{
		if(strcmp(cmd, commands[cmd_loop].name) == 0)
		{
			found_cmd = &commands[cmd_loop];
			break;
		}

		if(commands[cmd_loop].syn)
		{
			if(strcmp(cmd, commands[cmd_loop].syn) == 0)
			{
				found_cmd = &commands[cmd_loop];
				break;
			}
		}
	}

	return found_cmd;
}

int shellParse(char *command)
{
	int ret = CMD_OK;
	char *cmd;
	int argc;
	char *argv[16];
	char outbuf[MAX_BUFFER];

	if(parse_args(command, outbuf, &argc, argv, 16) == 0)
	{
		printf("Error parsing command\n");
		return CMD_ERROR;
	}

	if((argc > 0) && (argv[0][0] != '#'))
	{
		struct sh_command *found_cmd;

		cmd = argv[0];
		found_cmd = find_command(cmd);
		if((found_cmd) && (found_cmd->type != SHELL_TYPE_CATEGORY))
		{
			if((found_cmd->min_args > (argc - 1)) || ((ret = found_cmd->func(argc-1, &argv[1])) == CMD_ERROR))
			{
				printf("Usage: %s\n", found_cmd->help);
			}
		}
		else
		{
			printf("Unknown command %s\n", cmd);
			ret = CMD_ERROR;
		}
	}

	return ret;
}

/* Process command line */
static int process_cli()
{
	int ret;

    pspDebugSioPutchar(13);
    pspDebugSioPutchar(10);
	g_cli[g_cli_pos] = 0;
	g_cli_pos = 0;
	memcpy(&g_lastcli[g_lastcli_pos][0], g_cli, CLI_MAX);
	g_lastcli_pos = (g_lastcli_pos + 1) % CLI_HISTSIZE;
	g_currcli_pos = g_lastcli_pos;

	ret = shellParse(g_cli);
	if(ret != CMD_EXITSHELL)
	{
		print_prompt();
	}

	return ret;
}

/* Handle an escape sequence */
static void cli_handle_escape(void)
{
	char ch;

	ch = g_readcharwithtimeout();

	if(ch != -1)
	{
		/* Arrow keys UDRL/ABCD */
		if(ch == '[')
		{
			ch = g_readcharwithtimeout();
			switch(ch)
			{
				case 'A' : {
							   int pos;

							   pos = g_currcli_pos - 1;
							   if(pos < 0)
							   {
								   pos += CLI_HISTSIZE;
							   }

							   if(g_lastcli[pos][0] != 0)
							   {
								   char *src, *dst;

								   src = g_lastcli[pos];
								   dst = g_cli;
								   g_currcli_pos = pos;
								   g_cli_pos = 0;
								   g_cli_size = 0;
								   while(*src)
								   {
									   *dst++ = *src++;
									   g_cli_pos++;
									   g_cli_size++;
								   }
								   *dst = 0;

								   printf("\n");
								   print_prompt();
								   printf("%s", g_cli);
							   } 
						   } 
						   break;

				case 'B' : {
							   int pos;

							   pos = g_currcli_pos + 1;
							   pos %= CLI_HISTSIZE;

							   if(g_lastcli[pos][0] != 0)
							   {
								   char *src, *dst;

								   src = g_lastcli[pos];
								   dst = g_cli;
								   g_currcli_pos = pos;
								   g_cli_pos = 0;
								   g_cli_size = 0;
								   while(*src)
								   {
									   *dst++ = *src++;
									   g_cli_pos++;
									   g_cli_size++;
								   }
								   *dst = 0;

								   printf("\n");
								   print_prompt();
								   printf("%s", g_cli);
							   } 
						   } 
						   break;



				default: 
							printf("Unknown character %d\n", ch);
						   break;
			};
		}
		else
		{
			printf("Unknown character %d\n", ch);
		}
	}
}

/* Main shell function */
void shellStart(const char *cliprompt)
{		
	int exit_shell = 0;

	if(strlen(cliprompt) > 0)
	{
		set_shell_var("prompt", cliprompt);
	}

	strcpy(g_context.currdir, "ms0:/");
	print_prompt();
	g_cli_pos = 0;
	g_cli_size = 0;
	memset(g_cli, 0, CLI_MAX);

	while(!exit_shell) {
		char ch;

		ch = g_readchar();
		switch(ch)
		{
			case -1 : break; // No char
					  /* ^D */
			case 4  : printf("\nExiting Shell\n");
					  exit_shell = 1;
					  break;
			case 8  : // Backspace
	                case 127: if(g_cli_pos > 0)
					  {
						  g_cli_pos--;
						  g_cli[g_cli_pos] = 0;
						  pspDebugSioPutchar(8);
						  pspDebugSioPutchar(' ');
						  pspDebugSioPutchar(8);
					  }
					  break;
			case 9  : break; // Ignore tab
			case 13 :		 // Enter key 
			case 10 : if(process_cli() == CMD_EXITSHELL) 
					  {
						  exit_shell = 1;
					  }
					  break;
			case 27 : /* Escape character */
					  cli_handle_escape();
					  break;
			default : if((g_cli_pos < (CLI_MAX - 1)) && (ch >= 32))
					  {
						  g_cli[g_cli_pos++] = ch;
						  g_cli[g_cli_pos] = 0;
						  pspDebugSioPutchar(ch);
					  }
					  break;
		}
	}
}

/* Help command */
static int help_cmd(int argc, char **argv)
{
	int cmd_loop;

	if(argc < 1)
	{
		printf("Command Categories\n\n");
		for(cmd_loop = 0; commands[cmd_loop].name; cmd_loop++)
		{
			if(commands[cmd_loop].type == SHELL_TYPE_CATEGORY)
			{
				printf("%-10s - %s\n", commands[cmd_loop].name, commands[cmd_loop].desc);
			}
		}
		printf("\nType 'help category' for more information\n");
	}
	else
	{
		struct sh_command* found_cmd;

		found_cmd = find_command(argv[0]);
		if(found_cmd != NULL)
		{
			if(found_cmd->type == SHELL_TYPE_CATEGORY)
			{
				/* Print the commands listed under the separator */
				printf("Category %s\n\n", found_cmd->name);
				for(cmd_loop = 1; found_cmd[cmd_loop].name && found_cmd[cmd_loop].type != SHELL_TYPE_CATEGORY; cmd_loop++)
				{
					printf("%-10s - %s\n", found_cmd[cmd_loop].name, found_cmd[cmd_loop].desc);
					if((cmd_loop % 24) == 20)
					{
						char ch;
						printf("Press any key to continue, or q to quit\n");

						while((ch = g_readchar()) == -1);
						ch = toupper(ch);
						if(ch == 'Q')
						{
							break;
						}
					}
				}
			}
			else
			{
				printf("%s\t - %s\n", found_cmd->name, found_cmd->desc);
				if(found_cmd->syn)
				{
					printf("Synonym: %s\n", found_cmd->syn);
				}
				printf("Usage: %s\n", found_cmd->help);
			}
		}
		else
		{
			printf("Unknown command %s, type help for information\n", argv[0]);
		}
	}

	return CMD_OK;
}

