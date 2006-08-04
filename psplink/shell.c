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
#include <pspthreadman_kernel.h>
#include <psppower.h>
#include <stdint.h>
#include "memoryUID.h"
#include "psplink.h"
#include "psplinkcnf.h"
#include "parse_args.h"
#include "util.h"
#include "sio.h"
#include "bitmap.h"
#include "config.h"
#include "shell.h"
#include "script.h"
#include "version.h"
#include "exception.h"
#include "decodeaddr.h"
#include "debug.h"
#include "symbols.h"
#include "libs.h"
#include "thctx.h"
#include "disasm.h"
#include "apihook.h"
#include "tty.h"

#define MAX_SHELL_VAR      128
#define SHELL_PROMPT	"psplink %d>"
/* Maximum command line */
#define CLI_MAX			128
/* Maximum history */
#define CLI_HISTSIZE	8
/* Define the pass prompt value */
#define PASSPROMPT_VAL  0xFF

extern struct GlobalContext g_context;

int (*g_readchar)(void) = sioReadChar;
int (*g_readcharwithtimeout)(void) = sioReadCharWithTimeout;

typedef struct _CommandMsg
{
	struct _CommandMsg *link;
	char   *command;
	int    res;
} CommandMsg;

#ifndef USB_ONLY
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
/* Message box for command line parsing */
#endif 

static SceUID g_command_msg = -1;
/* Thread ID for the command line parsing */
static SceUID g_command_thid = -1;
/* Semaphore to lock the cli */
static SceUID g_cli_sema = -1;
/* Event flag to indicate the end of command parse */
static SceUID g_command_event = -1;
/* Indicates the name of the last module we loaded */
static char g_lastmod[32] = "";
/* Indicates we are in tty mode */
static int g_ttymode = 0;

#define COMMAND_EVENT_DONE 1

typedef int (*threadmanprint_func)(SceUID uid, int verbose);

struct shell_variable
{
	const char *name;
	char data[MAX_SHELL_VAR];
};

struct shell_variable g_shellvars[] = 
{
	{ "prompt", SHELL_PROMPT },
	{ "path", "" },
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
;
	strncpy(vardata, data, MAX_SHELL_VAR-1);
	vardata[MAX_SHELL_VAR-1] = 0;

	return 1;
}

void print_prompt(void)
{
	char tmp[MAX_SHELL_VAR];
	const char *cliprompt;
	int in, out;

	if(g_ttymode)
	{
		return;
	}

	cliprompt = find_shell_var("prompt");
	if(cliprompt == NULL)
	{
		printf("ERROR> ");
		return;
	}

	out = 0;
	in = 0;

	if(g_context.pcterm)
	{
		tmp[out++] = PASSPROMPT_VAL;
	}

	while((cliprompt[in]) && (out < (MAX_SHELL_VAR-2)))
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

	if(g_context.pcterm)
	{
		tmp[out++] = PASSPROMPT_VAL;
	}

	tmp[out] = 0;
	printf("%s ", tmp);
}

void psplinkPrintPrompt(void)
{
	u32 k1;

	k1 = psplinkSetK1(0);
	print_prompt();
	psplinkSetK1(k1);
}

static SceUID get_module_uid(const char *name)
{
	char *endp;
	SceUID uid = -1;

	if(name[0] == '@')
	{
		uid = refer_module_by_name(&name[1], NULL);
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
		if(strcmp(argv[0], "v") == 0)
		{
			verbose = 1;
		}
	}

	memset(ids, 0, 100 * sizeof(SceUID));
	ret = sceKernelGetThreadmanIdList(type, ids, 100, &count);
	if(ret >= 0)
	{
		printf("<%s List (%d entries)>\n", name, count);
		for(i = 0; i < count; i++)
		{
			if(pinfo(ids[i], verbose) < 0)
			{
				printf("ERROR: Unknown %s 0x%08X\n", name, ids[i]);
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
			printf("ERROR: Unknown %s 0x%08X\n", name, uid);
		}

		ret = CMD_OK;
	}

	return ret;
}

static const char* get_thread_status(int stat, char *str)
{
	str[0] = 0;
	if(stat & PSP_THREAD_RUNNING)
	{
		strcat(str, "RUNNING ");
	}
	
	if(stat & PSP_THREAD_READY)
	{
		strcat(str, "READY ");
	}

	if(stat & PSP_THREAD_WAITING)
	{
		strcat(str, "WAITING ");
	}
	
	if(stat & PSP_THREAD_SUSPEND)
	{
		strcat(str, "SUSPEND ");
	}

	if(stat & PSP_THREAD_STOPPED)
	{
		strcat(str, "STOPPED ");
	}

	if(stat & PSP_THREAD_KILLED)
	{
		strcat(str, "KILLED ");
	}

	return str;
}

static int print_threadinfo(SceUID uid, int verbose)
{
	SceKernelThreadInfo info;
	char status[256];
	char cwd[512];
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferThreadStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr: 0x%08X - Status: %d/%s- Entry: %p\n", info.attr, info.status, 
					get_thread_status(info.status, status), info.entry);
			printf("Stack: %p - StackSize 0x%08X - GP: 0x%08X\n", info.stack, info.stackSize,
					(u32) info.gpReg);
			printf("InitPri: %d - CurrPri: %d - WaitType %d\n", info.initPriority,
					info.currentPriority, info.waitType);
			printf("WaitId: 0x%08X - WakeupCount: %d - ExitStatus: 0x%08X\n", info.waitId,
					info.wakeupCount, info.exitStatus);
			printf("RunClocks: %d - IntrPrempt: %d - ThreadPrempt: %d\n", info.runClocks.low,
					info.intrPreemptCount, info.threadPreemptCount);
			printf("ReleaseCount: %d, StackFree: %d\n", info.releaseCount, sceKernelGetThreadStackFreeSize(uid));
			if(sceIoGetThreadCwd(uid, cwd, sizeof(cwd)) > 0)
			{
				printf("Current Dir: %s\n", cwd);
			}
		}
	}

	return ret;
}

static int thlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Thread, "Thread", print_threadinfo);
}

static int thsllist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_SleepThread, "Sleep Thread", print_threadinfo);
}

static int thdelist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_DelayThread, "Delay Thread", print_threadinfo);
}

static int thsulist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_SuspendThread, "Suspend Thread", print_threadinfo);
}

static int thdolist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_DormantThread, "Dormant Thread", print_threadinfo);
}

static int thinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Thread", print_threadinfo, (ReferFunc) pspSdkReferThreadStatusByName);
}

static int thread_do_cmd(const char *name, const char *type, ReferFunc refer, int (*fn)(SceUID uid))
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;

	uid = get_thread_uid(name, refer);

	if(uid >= 0)
	{
		err = fn(uid);
		if(err < 0)
		{
			printf("Cannot %s uid 0x%08X (error: 0x%08X)\n", type, uid, err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int thsusp_cmd(int argc, char **argv)
{
	return thread_do_cmd(argv[0], "suspend", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelSuspendThread);
}

static int thspuser_cmd(int argc, char **argv)
{
	sceKernelSuspendAllUserThreads();

	return CMD_OK;
}

static int thresm_cmd(int argc, char **argv)
{
	return thread_do_cmd(argv[0], "resume", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelResumeThread);
}

static int thwake_cmd(int argc, char **argv)
{
	return thread_do_cmd(argv[0], "wakeup", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelWakeupThread);
}

static int thterm_cmd(int argc, char **argv)
{
	return thread_do_cmd(argv[0], "terminate", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelTerminateThread);
}

static int thdel_cmd(int argc, char **argv)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelDeleteThread);
}

static int thtdel_cmd(int argc, char **argv)
{
	return thread_do_cmd(argv[0], "terminate delete", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelTerminateDeleteThread);
}

static int thctx_cmd(int argc, char **argv)
{
	return thread_do_cmd(argv[0], "get context", (ReferFunc) pspSdkReferThreadStatusByName, threadFindContext);
}

static int thpri_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;
	u32 pri;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferThreadStatusByName);

	if((uid >= 0) && (strtoint(argv[1], &pri)))
	{
		err = sceKernelChangeThreadPriority(uid, pri);
		if(err < 0)
		{
			printf("Cannot %s uid 0x%08X (error: 0x%08X)\n", "change priority", uid, err);
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
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr: 0x%08X - initPattern 0x%08X - currPatten 0x%08X\n", info.attr, info.initPattern, 
					info.currentPattern);
			printf("NumWaitThreads: 0x%08X\n", info.numWaitThreads);
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
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr: 0x%08X - initCount: 0x%08X - currCount: 0x%08X\n", info.attr, info.initCount, 
					info.currentCount);
			printf("maxCount: 0x%08X - NumWaitThreads: 0x%08X\n", info.maxCount, info.numWaitThreads);
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

static int print_mboxinfo(SceUID uid, int verbose)
{
	SceKernelMbxInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferMbxStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr: 0x%08X - numWaitThreads: 0x%08X - numMessages: 0x%08X\n", info.attr, info.numWaitThreads, 
					info.numMessages);
			printf("firstMessage %p\n", info.firstMessage);
		}
	}

	return ret;
}

static int mxlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Mbox, "Message Box", print_mboxinfo);
}

static int mxinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Message Box", print_mboxinfo, (ReferFunc) pspSdkReferMboxStatusByName);
}

static int print_cbinfo(SceUID uid, int verbose)
{
	SceKernelCallbackInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferCallbackStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("threadId 0x%08X - callback %p - common %p\n", info.threadId, info.callback, info.common);
			printf("notifyCount %d - notifyArg %d\n", info.notifyCount, info.notifyArg);
		}
	}

	return ret;
}

static int cblist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Callback, "Callback", print_cbinfo);
}

static int cbinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Callback", print_cbinfo, (ReferFunc) pspSdkReferCallbackStatusByName);
}

static int print_vtinfo(SceUID uid, int verbose)
{
	SceKernelVTimerInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferVTimerStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("active %d - base.hi %d - base.low %d - current.hi %d - current.low %d\n", 
				   info.active, info.base.hi, info.base.low, info.current.hi, info.current.low);	
			printf("schedule.hi %d - schedule.low %d - handler %p - common %p\n", info.schedule.hi,
					info.schedule.low, info.handler, info.common);
		}
	}

	return ret;
}

static int vtlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_VTimer, "VTimer", print_vtinfo);
}

static int vtinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "VTimer", print_vtinfo, (ReferFunc) pspSdkReferVTimerStatusByName);
}

static int print_vplinfo(SceUID uid, int verbose)
{
	SceKernelVplInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferVplStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr 0x%08X - poolSize %d - freeSize %d - numWaitThreads %d\n",
					info.attr, info.poolSize, info.freeSize, info.numWaitThreads);
		}
	}

	return ret;
}

static int vpllist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Vpl, "Vpl", print_vplinfo);
}

static int vplinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Vpl", print_vplinfo, (ReferFunc) pspSdkReferVplStatusByName);
}

static int print_fplinfo(SceUID uid, int verbose)
{
	SceKernelFplInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferFplStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr 0x%08X - blockSize %d - numBlocks %d - freeBlocks %d - numWaitThreads %d\n",
					info.attr, info.blockSize, info.numBlocks, info.freeBlocks, info.numWaitThreads);
		}
	}

	return ret;
}

static int fpllist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Fpl, "Fpl", print_fplinfo);
}

static int fplinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Fpl", print_fplinfo, (ReferFunc) pspSdkReferFplStatusByName);
}

static int print_mppinfo(SceUID uid, int verbose)
{
	SceKernelMppInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferMsgPipeStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("Attr 0x%08X - bufSize %d - freeSize %d\n", info.attr, info.bufSize, info.freeSize);
			printf("numSendWaitThreads %d - numReceiveWaitThreads %d\n", info.numSendWaitThreads,
					info.numReceiveWaitThreads);
		}
	}

	return ret;
}

static int mpplist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Mpipe, "Message Pipe", print_mppinfo);
}

static int mppinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Message Pipe", print_mppinfo, (ReferFunc) pspSdkReferMppStatusByName);
}

static int print_thevinfo(SceUID uid, int verbose)
{
	SceKernelThreadEventHandlerInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferThreadEventHandlerStatus(uid, &info);
	if(ret == 0)
	{
		printf("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			printf("threadId 0x%08X - mask %02X - handler %p\n", info.threadId, info.mask, info.handler);
			printf("common %p\n", info.common);
		}
	}

	return ret;
}

static int thevlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_ThreadEventHandler, "Thread Event Handler", print_thevinfo);
}

static int thevinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Thread Event Handler", print_thevinfo, (ReferFunc) pspSdkReferThreadEventHandlerStatusByName);
}

int thread_event_handler(int mask, SceUID thid, void *common)
{
	const char *event = "Unknown";
	const char *thname = "Unknown";
	SceKernelThreadInfo thinfo;

	switch(mask)
	{
		case THREAD_CREATE: event = "Create";
							break;
		case THREAD_START: event = "Start";
						   break;
		case THREAD_EXIT: event = "Exit";
						  break;
		case THREAD_DELETE: event = "Delete";
							break;
		default: break;
	};

	memset(&thinfo, 0, sizeof(thinfo));
	thinfo.size = sizeof(thinfo);
	if(sceKernelReferThreadStatus(thid, &thinfo) == 0)
	{
		thname = thinfo.name;
	}

	printf("Thread %-6s: thid 0x%08X name %s\n", event, thid, thname);

	return 0;
}

static int thmon_cmd(int argc, char **argv)
{
	SceUID ev;
	SceUID uid;
	int mask = 0;

	if(g_context.thevent >= 0)
	{
		sceKernelReleaseThreadEventHandler(g_context.thevent);
		g_context.thevent = -1;
	}

	switch(argv[0][0])
	{
		case 'a': uid = THREADEVENT_ALL;
				  break;
		case 'u': uid = THREADEVENT_USER;
				  break;
		case 'k': uid = THREADEVENT_KERN;
				  break;
		default: return CMD_ERROR;
	};

	if(argc > 1)
	{
		int loop;

		for(loop = 0; argv[1][loop]; loop++)
		{
			switch(argv[1][loop])
			{
				case 'c': mask |= THREAD_CREATE;
						  break;
				case 's': mask |= THREAD_START;
						  break;
				case 'e': mask |= THREAD_EXIT;
						  break;
				case 'd': mask |= THREAD_DELETE;
						  break;
				default: /* Do nothing */
						  break;
			};
		}
	}
	else
	{
		mask = THREAD_CREATE | THREAD_START | THREAD_EXIT | THREAD_DELETE;
	}

	ev = sceKernelRegisterThreadEventHandler("PSPLINK_THEV", uid, mask, thread_event_handler, NULL);
	g_context.thevent = ev;

	return CMD_OK;
}

static int thmonoff_cmd(int argc, char **argv)
{
	if(g_context.thevent >= 0)
	{
		sceKernelReleaseThreadEventHandler(g_context.thevent);
		g_context.thevent = -1;
	}

	return CMD_OK;
}

static int sysstat_cmd(int argc, char **argv)
{
	SceKernelSystemStatus stat;

	memset(&stat, 0, sizeof(stat));
	stat.size = sizeof(stat);

	if(!sceKernelReferSystemStatus(&stat))
	{
		printf("System Status: 0x%08X\n", stat.status);
		printf("Idle Clocks:   %08X%08X\n", stat.idleClocks.hi, stat.idleClocks.low);
		printf("Resume Count:  %d\n", stat.comesOutOfIdleCount);
		printf("Thread Switch: %d\n", stat.threadSwitchCount);
		printf("VFPU Switch:   %d\n", stat.vfpuSwitchCount);
	}

	return CMD_OK;
}

static int uidlist_cmd(int argc, char **argv)
{
	const char *name = NULL;

	if(argc > 0)
	{
		name = argv[0];
	}
	printUIDList(name);

	return CMD_OK;
}

static int uidinfo_cmd(int argc, char **argv)
{
	uidList *entry;
	const char *parent = NULL;

	if(argc > 1)
	{
		parent = argv[1];
	}

	if(argv[0][0] == '@')
	{
		entry = findObjectByNameWithParent(&argv[0][1], parent);
	}
	else
	{
		SceUID uid;

		uid = strtoul(argv[0], NULL, 0);
		entry = findObjectByUIDWithParent(uid, parent);
	}

	if(entry)
	{
		printUIDEntry(entry);
		if(entry->realParent)
		{
			printf("Parent:\n");
			printUIDEntry(entry->realParent);
		}
	}

	return CMD_OK;
}

static int cop0_cmd(int argc, char **argv)
{
	u32 regs[64];
	int i;

	psplinkGetCop0(regs);

	printf("MXC0 Regs:\n");
	for(i = 0; i < 32; i += 2)
	{
		printf("$%02d: 0x%08X  -  $%02d: 0x%08X\n", i, regs[i], i+1, regs[i+1]);
	}
	printf("\n");

	printf("CXC0 Regs:\n");
	for(i = 0; i < 32; i += 2)
	{
		printf("$%02d: 0x%08X  -  $%02d: 0x%08X\n", i, regs[i+32], i+1, regs[i+33]);
	}

	return CMD_OK;
}

static int print_modinfo(SceUID uid, int verbose)
{
	SceKernelModuleInfo info;
	PspDebugPutChar kp;
	int ret;

	kp = sioDisableKprintf();
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	ret = g_QueryModuleInfo(uid, &info);
	if(ret >= 0)
	{
		int i;
		printf("UID: 0x%08X Attr: %04X - Name: %s\n", uid, info.attribute, info.name);
		if(verbose)
		{
			printf("Entry: 0x%08X - GP: 0x%08X - TextAddr: 0x%08X\n", info.entry_addr,
					info.gp_value, info.text_addr);
			printf("TextSize: 0x%08X - DataSize: 0x%08X BssSize: 0x%08X\n", info.text_size,
					info.data_size, info.bss_size);
			for(i = 0; (i < info.nsegment) && (i < 4); i++)
			{
				printf("Segment %d: Addr 0x%08X - Size 0x%08X\n", i, 
						(u32) info.segmentaddr[i], (u32) info.segmentsize[i]);
			}
		}
	}
	sioEnableKprintf(kp);


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
			printf("ERROR: Unknown module 0x%08X\n", uid);
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
		printf("<Module List (%d modules)>\n", count);
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
		printf("Module Stop 0x%08X Status 0x%08X\n", uid_ret, status);

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
		printf("Module Unload 0x%08X\n", uid_ret);

		ret = CMD_OK;
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;

}

static int modstun_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		SceUID stop, unld;
		int status;

		stop = sceKernelStopModule(uid, 0, NULL, &status, NULL);
		unld = sceKernelUnloadModule(uid);
		printf("Module Stop/Unload 0x%08X/0x%08X Status 0x%08X\n", stop, unld, status);

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
		printf("Module Start 0x%08X Status 0x%08X\n", uid_ret, status);

		ret = CMD_OK;
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modexp_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(libsPrintEntries(uid))
		{
			ret = CMD_OK;
		}
		else
		{
			printf("ERROR: Couldn't find module %s\n", argv[0]);
		}
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modimp_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(libsPrintImports(uid))
		{
			ret = CMD_OK;
		}
		else
		{
			printf("ERROR: Couldn't find module %s\n", argv[0]);
		}
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modfindx_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;
	u32 addr = 0;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(argv[2][0] == '@')
		{
			addr = libsFindExportByName(uid, argv[1], &argv[2][1]);
		}
		else
		{
			char *endp;
			u32 nid;

			nid = strtoul(argv[2], &endp, 16);
			if(*endp != 0)
			{
				printf("ERROR: Invalid nid %s\n", argv[2]);
			}
			else
			{
				addr = libsFindExportByNid(uid, argv[1], nid);
			}
		}
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	if(addr != 0)
	{
		printf("Library: %s, Exp %s, Addr: 0x%08X\n", argv[1], argv[2], addr);

		ret = CMD_OK;
	}
	else
	{
		printf("Couldn't find module export\n");
	}

	return ret;
}

static int apihook_common(int argc, char **argv, int sleep)
{
	SceUID uid;
	int ret = CMD_ERROR;
	const char *param = "";

	if(argc > 4)
	{
		param = argv[4];
	}

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(argv[2][0] == '@')
		{
			if(apiHookGenericByName(uid, argv[1], &argv[2][1], argv[3][0], param, sleep))
			{
				ret = CMD_OK;
			}
		}
		else
		{
			char *endp;
			u32 nid;

			nid = strtoul(argv[2], &endp, 16);
			if(*endp != 0)
			{
				printf("ERROR: Invalid nid %s\n", argv[2]);
			}
			else
			{
				if(apiHookGenericByNid(uid, argv[1], nid, argv[3][0], param, 0))
				{
					ret = CMD_OK;
				}
			}
		}
	}
	else
	{
		printf("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int apihook_cmd(int argc, char **argv)
{
	return apihook_common(argc, argv, 0);
}

static int apihooks_cmd(int argc, char **argv)
{
	return apihook_common(argc, argv, 1);
}

static int apihp_cmd(int argc, char **argv)
{
	apiHookGenericPrint();
	return CMD_OK;
}

static int apihd_cmd(int argc, char **argv)
{
	u32 id;

	if(strtoint(argv[0], &id))
	{
		apiHookGenericDelete(id);
	}
	else
	{
		printf("Invalid ID for delete\n");
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int modload_cmd(int argc, char **argv)
{
	SceUID modid;
	SceKernelModuleInfo info;
	char path[1024];

	if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
	{
		modid = sceKernelLoadModule(path, 0, NULL);
		if(!psplinkReferModule(modid, &info))
		{
			printf("Module Load '%s' UID: 0x%08X\n", path, modid);
		}
		else
		{
			printf("Module Load '%s' UID: 0x%08X Name: %s\n", path, modid, info.name);
			strncpy(g_lastmod, info.name, 31);
			g_lastmod[31] = 0;
		}
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
	char *file = NULL;
	char *key = NULL;
	int  len;
	struct SceKernelLoadExecParam le;

	if(argv[0][0] == '@')
	{
		key = &argv[0][1];
		if(argc < 2)
		{
			return CMD_ERROR;
		}
		file = argv[1];
	}
	else
	{
		file = argv[0];
	}

	if(handlepath(g_context.currdir, file, path, TYPE_FILE, 1))
	{
		len = build_args(args, path, argc - 1, &argv[1]);
		le.size = sizeof(le);
		le.args = len;
		le.argp = args;
		le.key = key;

		psplinkStop();

		sceKernelLoadExec(path, &le);
	}

	return CMD_OK;
}

static int ldstart_cmd(int argc, char **argv)
{
	char path[MAXPATHLEN];
	SceKernelModuleInfo info;
	int ret = CMD_ERROR;

	if(argc > 0)
	{
		SceUID modid;

		if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
		{
			modid = load_start_module(path, argc-1, &argv[1]);
			if(modid >= 0)
			{
				if(!psplinkReferModule(modid, &info))
				{
					printf("Load/Start %s UID: 0x%08X\n", path, modid);
				}
				else
				{
					printf("Load/Start %s UID: 0x%08X Name: %s\n", path, modid, info.name);
					strncpy(g_lastmod, info.name, 31);
					g_lastmod[31] = 0;
				}
			}
			else
			{
				printf("Failed to Load/Start module '%s' Error: 0x%08X\n", path, modid);
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

static int kill_cmd(int argc, char **argv)
{
	SceUID uid;
	int ret = CMD_ERROR;

	do
	{
		uid = get_module_uid(argv[0]);
		if(uid >= 0)
		{
			SceUID thids[100];
			SceKernelThreadInfo info;
			SceKernelModuleInfo modinfo;
			int count;
			int i;
			int status;
			int error;

			error = sceKernelStopModule(uid, 0, NULL, &status, NULL);
			if(error < 0)
			{
				printf("Error could not stop module 0x%08X\n", error);
				break;
			}

			printf("Stop status %08X\n", status);
			memset(thids, 0, sizeof(thids));
			if(sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, thids, 100, &count) >= 0)
			{
				for(i = 0; i < count; i++)
				{
					memset(&info, 0, sizeof(info));
					info.size = sizeof(info);
					if(sceKernelReferThreadStatus(thids[i], &info) < 0)
					{
						continue;
					}

					if(refer_module_by_addr((u32) info.entry, &modinfo) == uid)
					{
						sceKernelTerminateDeleteThread(thids[i]);
					}
				}
			}

			if(sceKernelUnloadModule(uid) < 0)
			{
				printf("Error could not unload module\n");
				break;
			}

			ret = CMD_OK;
		}
	}
	while(0);

	return ret;
}

static int debug_cmd(int argc, char **argv)
{
	char path[1024];
	int  ret = CMD_ERROR;

	if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
	{
		if((!g_context.usbgdb) && (g_context.wifi == 0))
		{
			/* Default to AP 1 */
			load_wifi(g_context.bootpath, 1);
		}

		if(g_context.gdb == 0)
		{
			argv[0] = path;
			load_gdb(g_context.bootpath, argc, argv);
		}
		else
		{
			printf("Error GDB already running, please reset\n");
		}

		ret = CMD_OK;
	}
	else
	{
		printf("Error invalid file %s\n", path);
	}

	return ret;
}

static int calc_cmd(int argc, char **argv)
{
	u32 val;
	char disp;

	if(memDecode(argv[0], &val))
	{
		if(argc > 1)
		{
			disp = upcase(argv[1][0]);
		}
		else
		{
			disp = 'X';
		}

		switch(disp)
		{
			case 'D': printf("Result = %d\n", val);
					  break;
			case 'O': printf("Result = %o\n", val);
					  break;
			default :
			case 'X': printf("Result = 0x%08X\n", val);
					  break;
		};
	}
	else
	{
		printf("Error could not calculate address\n");
	}

	return CMD_OK;
}

static int reset_cmd(int argc, char **argv)
{
	if(argc > 0)
	{
		if(strcmp(argv[0], "game") == 0)
		{
			g_context.rebootkey = REBOOT_MODE_GAME;
		}
		else if(strcmp(argv[0], "vsh") == 0)
		{
			g_context.rebootkey = REBOOT_MODE_VSH;
		}
		else if(strcmp(argv[0], "updater") == 0)
		{
			g_context.rebootkey = REBOOT_MODE_VSH;
		}
	}

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

			psplinkStop();

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
				printf("Load/Start module UID: 0x%08X\n", modid);
				strcpy(g_context.execfile, file);
				g_context.inexec = 1;
				save_execargs(argc-1, &argv[1]);
				ret = CMD_OK;
			}
			else
			{
				printf("Failed to Load/Start module '%s' Error: 0x%08X\n", file, modid);
			}
		}
	}
	while(0);

	return ret;
}

static int list_dir(const char *name)
{
	char buffer[512];
	char *p = buffer;
	int dfd;
	static SceIoDirent dir;

	dfd = sceIoDopen(name);
	if(dfd >= 0)
	{
		memset(&dir, 0, sizeof(dir));
		while(sceIoDread(dfd, &dir) > 0)
		{
			int ploop;
			p = buffer;

			if(dir.d_stat.st_attr & FIO_SO_IFDIR)
			{
				*p++ = 'd';
			}
			else
			{
				*p++ = '-';
			}

			for(ploop = 2; ploop >= 0; ploop--)
			{
				int bits;

				bits = (dir.d_stat.st_mode >> (ploop * 3)) & 0x7;
				if(bits & 4)
				{
					*p++ = 'r';
				}
				else
				{
					*p++ = '-';
				}

				if(bits & 2)
				{
					*p++ = 'w';
				}
				else
				{
					*p++ = '-';
				}

				if(bits & 1)
				{
					*p++ = 'x';
				}
				else
				{
					*p++ = '-';
				}
			}

			sprintf(p, " %8d ", (int) dir.d_stat.st_size);
			p += strlen(p);
			sprintf(p, "%02d-%02d-%04d %02d:%02d ", dir.d_stat.st_mtime.day, 
					dir.d_stat.st_mtime.month, dir.d_stat.st_mtime.year,
					dir.d_stat.st_mtime.hour, dir.d_stat.st_mtime.minute);
			p += strlen(p);
			sprintf(p, "%s", dir.d_name);
			printf("%s\n", buffer);
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

static int usbmasson_cmd(int argc, char **argv)
{
	(void) init_usbmass();

	return CMD_OK;
}

static int usbmassoff_cmd(int argc, char **argv)
{
	(void) stop_usbmass();

	return CMD_OK;
}

static int usbhoston_cmd(int argc, char **argv)
{
	(void) init_usbhost(g_context.bootpath);

	return CMD_OK;
}

static int usbhostoff_cmd(int argc, char **argv)
{
	(void) stop_usbhost();

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
	int i;

	for(i = 0; i < argc; i++)
	{
		file = argv[0];

		if( !handlepath(g_context.currdir, file, afile, TYPE_FILE, 1) )
			continue;

		if( sceIoRemove(afile) < 0 )
			continue;

		printf("rm %s\n", afile);
	}

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
	char *slash;

	char fsrc[MAXPATHLEN];
	char fdst[MAXPATHLEN];
	char buff[2048];

	source = argv[0];
	destination = argv[1];

	if( !handlepath(g_context.currdir, source, fsrc, TYPE_FILE, 1) )
		return CMD_ERROR;
	
	if( !handlepath(g_context.currdir, destination, fdst, TYPE_ETHER, 0) )
		return CMD_ERROR;

	if(isdir(fdst))
	{
		int len;

		len = strlen(fdst);
		if((len > 0) && (fdst[len-1] != '/'))
		{
			strcat(fdst, "/");
		}

		slash = strrchr(fsrc, '/');
		strcat(fdst, slash+1);
	}

	printf("cp %s -> %s\n", fsrc, fdst);

	in = sceIoOpen(fsrc, PSP_O_RDONLY, 0777);
	if(in < 0)
	{
		printf("Couldn't open source file %s, 0x%08X\n", fsrc, in);
		return CMD_ERROR;
	}

	out = sceIoOpen(fdst, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if(out < 0)
	{
		sceIoClose(in);
		printf("Couldn't open destination file %s, 0x%08X\n", fdst, out);
		return CMD_ERROR;
	}

	while(1) {
		n = sceIoRead(in, buff, 2048);

		if(n <= 0)
			break;
		
		sceIoWrite(out, buff, n);
	}
	
	sceIoClose(in);
	sceIoClose(out);

	return CMD_OK;
}

static int remap_cmd(int argc, char **argv)
{
	int ret;

	sceIoUnassign(argv[1]);

	ret = sceIoAssign(argv[1], argv[0], NULL, IOASSIGN_RDWR, NULL, 0);
	if(ret < 0)
	{
		printf("Error remapping %s to %s, %08X\n", argv[0], argv[1], ret);
	}

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
	printf("N |    BASE    |   SIZE   | TOTALFREE |  MAXFREE  | ATTR |\n");
	printf("--|------------|----------|-----------|-----------|------|\n");
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
		printf("%d | 0x%08X | %8d | %9d | %9d | %04X |\n", 
				i, info.startaddr, info.memsize, total, free, info.attr);
	}

	return CMD_OK;
}

static int memreg_cmd(int argc, char **argv)
{
	memPrintRegions();
	return CMD_OK;
}

static int memblocks_cmd(int argc, char **argv)
{
	if(argc > 0)
	{
		switch(argv[0][0])
		{
			case 'f': sceKernelSysMemDump();
					  break;
			case 't': sceKernelSysMemDumpTail();
					  break;
			default: return CMD_ERROR;
					 
		};
	}
	else
	{
		sceKernelSysMemDumpBlock();
	}

	return CMD_OK;
}

/* Maximum memory dump size (per screen) */
#define MAX_MEMDUMP_SIZE 256
#define MEMDUMP_TYPE_BYTE 1
#define MEMDUMP_TYPE_HALF 2
#define MEMDUMP_TYPE_WORD 3

/* Print a row of a memory dump, up to row_size */
static void print_row(const u32* row, s32 row_size, u32 addr, int type)
{
	char buffer[128];
	char *p = buffer;
	int i = 0;

	sprintf(p, "%08x - ", addr);
	p += strlen(p);

	if(type == MEMDUMP_TYPE_WORD)
	{
		for(i = 0; i < 16; i+=4)
		{
			if(i < row_size)
			{
				sprintf(p, "%02X%02X%02X%02X ", row[i+3], row[i+2], row[i+1], row[i]);
			}
			else
			{
				sprintf(p, "-------- ");
			}
			p += strlen(p);
		}
	}
	else if(type == MEMDUMP_TYPE_HALF)
	{
		for(i = 0; i < 16; i+=2)
		{
			if(i < row_size)
			{
				sprintf(p, "%02X%02X ", row[i+1], row[i]);
			}
			else
			{
				sprintf(p, "---- ");
			}

			p += strlen(p);
		}
	}
	else
	{
		for(i = 0; i < 16; i++)
		{
			if(i < row_size)
			{
				sprintf(p, "%02X ", row[i]);
			}
			else
			{
				sprintf(p, "-- ");
			}

			p += strlen(p);
		}
	}

	sprintf(p, "- ");
	p += strlen(p);

	for(i = 0; i < 16; i++)
	{
		if(i < row_size)
		{
			if((row[i] >= 32) && (row[i] < 127))
			{
				*p++ = row[i];
			}
			else
			{
				*p++ =  '.';
			}
		}
		else
		{
			*p++ = '.';
		}
	}
	*p = 0;

	printf("%s\n", buffer);
}

/* Print a memory dump to SIO */
static void print_memdump(u32 addr, s32 size, int type)
{
	int size_left;
	u32 row[16];
	int row_size;
	u8 *p_addr = (u8 *) addr;

	if(type == MEMDUMP_TYPE_WORD)
	{
		printf("         - 00       04       08       0c       - 0123456789abcdef\n");
		printf("-----------------------------------------------------------------\n");
	}
	else if(type == MEMDUMP_TYPE_HALF)
	{
		printf("         - 00   02   04   06   08   0a   0c   0e   - 0123456789abcdef\n");
		printf("---------------------------------------------------------------------\n");
	}
	else 
	{
		printf("         - 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f - 0123456789abcdef\n");
		printf("-----------------------------------------------------------------------------\n");
	}

	size_left = size > MAX_MEMDUMP_SIZE ? MAX_MEMDUMP_SIZE : size;
	row_size = 0;

	while(size_left > 0)
	{
		row[row_size] = p_addr[row_size];
		row_size++;
		if(row_size == 16)
		{
			// draw row
			print_row(row, row_size, (u32) p_addr, type);
			p_addr += 16;
			row_size = 0;
		}

		size_left--;
	}
}

static int memdump_cmd(int argc, char **argv)
{
	static u32 addr = 0;
	static int type = MEMDUMP_TYPE_BYTE;
	s32 size_left;

	/* Get memory address */
	if(argc > 0)
	{
		if(argv[0][0] == '-')
		{
			addr -= MAX_MEMDUMP_SIZE;
		}
		else
		{
			if(!memDecode(argv[0], &addr))
			{
				printf("Error, invalid memory address %s\n", argv[0]);
				return CMD_ERROR;
			}
		}

		if(argc > 1)
		{
			if(argv[1][0] == 'w')
			{
				type = MEMDUMP_TYPE_WORD;
			}
			else if(argv[1][0] == 'h')
			{
				type = MEMDUMP_TYPE_HALF;
			}
			else if(argv[1][0] == 'b')
			{
				type = MEMDUMP_TYPE_BYTE;
			}
		}
	}
	else if(addr == 0)
	{
		return CMD_ERROR;
	}
	else
	{
		addr += MAX_MEMDUMP_SIZE;
	}

	size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);

	if(size_left > 0)
	{
		print_memdump(addr, size_left, type);
	}
	else
	{
		printf("Invalid memory address %x\n", addr);
	}

	return CMD_OK;
}

static int savemem_cmd(int argc, char **argv)
{
	char path[1024];
	u32 addr;
	int size;
	int written;
	char *endp;

	if(!handlepath(g_context.currdir, argv[2], path, TYPE_FILE, 0))
	{
		printf("Error invalid path\n");
		return CMD_ERROR;
	}

	size = strtoul(argv[1], &endp, 0);
	if(*endp != 0)
	{
		printf("Size parameter invalid '%s'\n", argv[1]);
		return CMD_ERROR;
	}

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int fd;

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size > size_left ? size_left : size;
		fd = sceIoOpen(path, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
		if(fd < 0)
		{
			printf("Could not open file '%s' for writing 0x%08X\n", path, fd);
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
	char path[1024];
	u32 addr;
	int maxsize = -1;
	char *endp;

	if(!handlepath(g_context.currdir, argv[1], path, TYPE_FILE, 1))
	{
		printf("Error invalid path\n");
		return CMD_ERROR;
	}

	if(argc > 2)
	{
		maxsize = strtoul(argv[2], &endp, 0);
		if(*endp != 0)
		{
			printf("Size parameter invalid '%s'\n", argv[2]);
			return CMD_ERROR;
		}
	}

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int readbytes;
		int fd;

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
		if(fd < 0)
		{
			printf("Could not open file '%s' for reading 0x%08X\n", path, fd);
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

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int i;

		addr &= ~3;
		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_WORD);
		if(size_left >= sizeof(u32))
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
				if(size_left <= 0)
				{
					break;
				}
			}
		}
		else
		{
			printf("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int pokeh_cmd(int argc, char **argv)
{
	u32 addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int i;

		addr &= ~1;
		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_HALF);
		if(size_left >= sizeof(u16))
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
				if(size_left <= 0)
				{
					break;
				}
			}
		}
		else
		{
			printf("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int pokeb_cmd(int argc, char **argv)
{
	u32 addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int i;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
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
				if(size_left <= 0)
				{
					break;
				}
			}
		}
		else
		{
			printf("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int peekw_cmd(int argc, char **argv)
{
	u32 addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		char fmt = 'x';

		if(argc > 1)
		{
			fmt = argv[1][0];
		}

		addr &= ~3;
		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_WORD);
		if(size_left >= sizeof(u32))
		{
			switch(fmt)
			{
				case 'f': {
							  char floatbuf[64];
							  float *pdata;

							  pspSdkDisableFPUExceptions();
							  pdata = (float *) addr;
							  f_cvt(*pdata, floatbuf, sizeof(floatbuf), 6, MODE_GENERIC);
							  printf("0x%08X: %s\n", addr, floatbuf);
						  }
						  break;
				case 'd': printf("0x%08X: %d\n", addr, _lw(addr));
						  break;
				case 'o': printf("0x%08X: %o\n", addr, _lw(addr));
						  break;
				case 'x':
				default:  printf("0x%08X: 0x%08X\n", addr, _lw(addr));
						  break;
			};
		}
		else
		{
			printf("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int peekh_cmd(int argc, char **argv)
{
	u32 addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		char fmt = 'x';

		if(argc > 1)
		{
			fmt = argv[1][0];
		}

		addr &= ~1;
		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_HALF);
		if(size_left >= sizeof(u16))
		{
			switch(fmt)
			{
				case 'd': printf("0x%08X: %d\n", addr, _lh(addr));
						  break;
				case 'o': printf("0x%08X: %o\n", addr, _lh(addr));
						  break;
				case 'x':
				default:  printf("0x%08X: 0x%04X\n", addr, _lh(addr));
						  break;
			};
		}
		else
		{
			printf("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int peekb_cmd(int argc, char **argv)
{
	u32 addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		char fmt = 'x';

		if(argc > 1)
		{
			fmt = argv[1][0];
		}

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		if(size_left > 0)
		{
			switch(fmt)
			{
				case 'd': printf("0x%08X: %d\n", addr, _lb(addr));
						  break;
				case 'o': printf("0x%08X: %o\n", addr, _lb(addr));
						  break;
				case 'x':
				default:  printf("0x%08X: 0x%02X\n", addr, _lb(addr));
						  break;
			};
		}
		else
		{
			printf("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int fillb_cmd(int argc, char **argv)
{
	u32 addr;
	u32 size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		u32 size_left;
		u32 val;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
		size = size > size_left ? size_left : size;

		if(strtoint(argv[2], &val) == 0)
		{
			printf("Invalid fill value %s\n", argv[2]);
			return CMD_ERROR;
		}

		memset((void *) addr, val, size);
	}

	return CMD_OK;
}

static int fillh_cmd(int argc, char **argv)
{
	u32 addr;
	u32 size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		u32 size_left;
		u32 val;
		int i;
		u16 *ptr;

		addr &= ~1;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_HALF);
		size = size > size_left ? size_left : size;

		if(strtoint(argv[2], &val) == 0)
		{
			printf("Invalid fill value %s\n", argv[2]);
			return CMD_ERROR;
		}

		ptr = (u16*) addr;

		for(i = 0; i < (size / 2); i++)
		{
			ptr[i] = (u16) val;
		}
	}

	return CMD_OK;
}

static int fillw_cmd(int argc, char **argv)
{
	u32 addr;
	u32 size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		u32 size_left;
		u32 val;
		int i;
		u32 *ptr;

		addr &= ~3;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_WORD);
		size = size > size_left ? size_left : size;

		if(strtoint(argv[2], &val) == 0)
		{
			printf("Invalid fill value %s\n", argv[2]);
			return CMD_ERROR;
		}

		ptr = (u32*) addr;

		for(i = 0; i < (size / 4); i++)
		{
			ptr[i] = (u32) val;
		}
	}

	return CMD_OK;
}

static int findstr_cmd(int argc, char **argv)
{
	u32 addr;
	u32 size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		u32 size_left;
		int searchlen;
		void *curr, *found;

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size_left > size ? size : size_left;
		searchlen = strlen(argv[2]);
		curr = (void *) addr;
		
		do
		{
			found = memmem_mask(curr, NULL, size, argv[2], searchlen);
			if(found)
			{
				printf("Found match at address 0x%p\n", found);
				found++;
				size -= (found - curr);
				curr = found;
			}
		}
		while((found) && (size > 0));
	}

	return CMD_OK;
}

static int findw_cmd(int argc, char **argv)
{
	u32 addr;
	u32 size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		u32 size_left;
		int searchlen;
		void *curr, *found;
		uint8_t search[128];
		int i;

		searchlen = 0;
		for(i = 2; i < argc; i++)
		{
			u32 val;

			if(strtoint(argv[i], &val) == 0)
			{
				printf("Invalid search value %s\n", argv[i]);
				return CMD_ERROR;
			}

			memcpy(&search[searchlen], &val, sizeof(val));
			searchlen += sizeof(val);
		}

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size_left > size ? size : size_left;
		curr = (void *) addr;
		
		do
		{
			found = memmem_mask(curr, NULL, size, search, searchlen);
			if(found)
			{
				printf("Found match at address 0x%p\n", found);
				found++;
				size -= (found - curr);
				curr = found;
			}
		}
		while((found) && (size > 0));
	}

	return CMD_OK;
}

static int findh_cmd(int argc, char **argv)
{
	u32 addr;
	u32 size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		u32 size_left;
		int searchlen;
		void *curr, *found;
		uint8_t search[128];
		int i;

		searchlen = 0;
		for(i = 2; i < argc; i++)
		{
			u32 val;

			if(strtoint(argv[i], &val) == 0)
			{
				printf("Invalid search value %s\n", argv[i]);
				return CMD_ERROR;
			}

			memcpy(&search[searchlen], &val, sizeof(u16));
			searchlen += sizeof(u16);
		}

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size_left > size ? size : size_left;
		curr = (void *) addr;
		
		do
		{
			found = memmem_mask(curr, NULL, size, search, searchlen);
			if(found)
			{
				printf("Found match at address 0x%p\n", found);
				found++;
				size -= (found - curr);
				curr = found;
			}
		}
		while((found) && (size > 0));
	}

	return CMD_OK;
}

static int findhex_cmd(int argc, char **argv)
{
	u32 addr;
	u32 size;
	uint8_t hex[128];
	uint8_t *mask = NULL;
	uint8_t mask_d[128];
	int hexsize;
	int masksize;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		u32 size_left;
		void *curr, *found;

		hexsize = decode_hexstr(argv[2], hex, sizeof(hex));
		if(hexsize == 0)
		{
			printf("Error in search string\n");
			return CMD_ERROR;
		}

		if(argc > 3)
		{
			masksize = decode_hexstr(argv[4], mask_d, sizeof(mask_d));
			if(masksize == 0)
			{
				printf("Error in mask string\n");
				return CMD_ERROR;
			}

			if(masksize != hexsize)
			{
				printf("Hex and mask do not match\n");
				return CMD_ERROR;
			}

			mask = mask_d;
		}

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size_left > size ? size : size_left;
		curr = (void *) addr;
		
		do
		{
			found = memmem_mask(curr, mask, size, hex, hexsize);
			if(found)
			{
				printf("Found match at address 0x%p\n", found);
				found++;
				size -= (found - curr);
				curr = found;
			}
		}
		while((found) && (size > 0));
	}

	return CMD_OK;
}

static int copymem_cmd(int argc, char **argv)
{
	u32 src;
	u32 dest;
	u32 size;

	if((memDecode(argv[0], &src)) && (memDecode(argv[1], &dest)) && memDecode(argv[2], &size))
	{
		u32 size_left;
		u32 srcsize;
		u32 destsize;

		srcsize = memValidate(src, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
		destsize = memValidate(dest, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
		size_left = srcsize > destsize ? destsize : srcsize;
		size = size > size_left ? size_left : size;

		memmove((void *) dest, (void *) src, size);
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

	if(memDecode(argv[0], &addr))
	{
		int size_left;

		addr &= ~3;
		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_WORD | MEM_ATTRIB_EXEC);
		if((size_left / 4) < count)
		{
			count = size_left / 4;
		}

		for(i = 0; i < count; i++)
		{
			printf("%s\n", disasmInstruction(_lw(addr), addr, NULL));
			addr += 4;
		}
	}

	return CMD_OK;
}

static int memprot_cmd(int argc, char **argv)
{
	if(strcmp(argv[0], "on") == 0)
	{
		memSetProtoff(0);
	}
	else if(strcmp(argv[0], "off") == 0)
	{
		memSetProtoff(1);
	}
	else
	{
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int disset_cmd(int argc, char **argv)
{
	disasmSetOpts(argv[0], 1);

	return CMD_OK;
}

static int disclear_cmd(int argc, char **argv)
{
	disasmSetOpts(argv[0], 0);

	return CMD_OK;
}

static int disopts_cmd(int argc, char **argv)
{
	printf("Disassembler Options: %s\n", disasmGetOpts());

	return CMD_OK;
}

static int scrshot_cmd(int argc, char **argv)
{
	char path[1024];
	SceUID block_id;
	void *block_addr;
	void *frame_addr;
	int frame_width;
	int pixel_format;
	int sync = 1;

	if(!handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 0))
	{
		printf("Error invalid path\n");
		return CMD_ERROR;
	}

	block_id = sceKernelAllocPartitionMemory(4, "scrshot", PSP_SMEM_Low, 512*1024, NULL);
	if(block_id < 0)
	{
		printf("Error could not allocate memory buffer 0x%08X\n", block_id);
		return CMD_ERROR;
	}
	 
	block_addr = sceKernelGetBlockHeadAddr(block_id);

	sceDisplayGetFrameBuf(&frame_addr, &frame_width, &pixel_format, &sync);
	printf("frame_addr %p, frame_width %d, pixel_format %d output %s\n", frame_addr, frame_width, pixel_format, path);

	if(frame_addr != NULL)
	{
		bitmapWrite((void *) ((u32) frame_addr | 0x40000000), block_addr, pixel_format, path);
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
		ret = scriptRun(path, argc, argv, g_lastmod, 0);
	}
	else
	{
		printf("Invalid file %s\n", path);
	}

	return ret;
}

static int dcache_cmd(int argc, char **argv)
{
	u32 addr = 0;
	u32 size = 0;
	void (*cacheall)(void);
	void (*cacherange)(const void *addr, unsigned int size);

	if(argc == 2)
	{
		printf("Must specify a size\n");
		return CMD_ERROR;
	}

	if(strcmp(argv[0], "w") == 0)
	{
		cacheall = sceKernelDcacheWritebackAll;
		cacherange = sceKernelDcacheWritebackRange;
	}
	else if(strcmp(argv[0], "i") == 0)
	{
		cacheall = sceKernelDcacheInvalidateAll;
		cacherange = sceKernelDcacheInvalidateRange;
	}
	else if(strcmp(argv[0], "wi") == 0)
	{
		cacheall = sceKernelDcacheWritebackInvalidateAll;
		cacherange = sceKernelDcacheWritebackInvalidateRange;
	}
	else
	{
		printf("Invalid type specifier '%s'\n", argv[0]);
		return CMD_ERROR;
	}

	if(argc > 1)
	{
		if(!memDecode(argv[1], &addr))
		{
			printf("Invalid address\n");
			return CMD_ERROR;
		}

		if(!strtoint(argv[2], &size))
		{
			printf("Invalid size argument\n");
			return CMD_ERROR;
		}

		cacherange((void *) addr, size);
	}
	else
	{
		cacheall();
	}

	return CMD_OK;
}

static int icache_cmd(int argc, char **argv)
{
	u32 addr = 0;
	u32 size = 0;

	if(argc == 1)
	{
		printf("Must specify a size\n");
		return CMD_ERROR;
	}

	if(argc > 0)
	{
		if(!memDecode(argv[0], &addr))
		{
			printf("Invalid address\n");
			return CMD_ERROR;
		}

		if(!strtoint(argv[1], &size))
		{
			printf("Invalid size argument\n");
			return CMD_ERROR;
		}

		sceKernelIcacheInvalidateRange((void *) addr, size);
	}
	else
	{
		sceKernelIcacheInvalidateAll();
	}

	return CMD_OK;
}

static int modaddr_cmd(int argc, char **argv)
{
	u32 addr;
	SceModule *pMod;

	if(memDecode(argv[0], &addr))
	{
		pMod = sceKernelFindModuleByAddress(addr);
		if(pMod != NULL)
		{
			print_modinfo(pMod->modid, 1);
		}
		else
		{
			printf("Couldn't find module at address 0x%08X\n", addr);
		}
	}
	else
	{
		printf("Invalid address %s\n", argv[0]);
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int exprint_cmd(int argc, char **argv)
{
	int ex = -1;

	if(argc > 0)
	{
		ex = atoi(argv[0]);
	}
	exceptionPrint(ex);

	return CMD_OK;
}

static int exlist_cmd(int argc, char **argv)
{
	exceptionList();

	return CMD_OK;
}

static int exctx_cmd(int argc, char **argv)
{
	exceptionSetCtx(atoi(argv[0]));

	return CMD_OK;
}

static int exprfpu_cmd(int argc, char **argv)
{
	int ex = -1;

	if(argc > 0)
	{
		ex = atoi(argv[0]);
	}
	exceptionFpuPrint(ex);

	return CMD_OK;
}

static int exprvfpu_cmd(int argc, char **argv)
{
	int ex = -1;
	int type = VFPU_PRINT_SINGLE;

	if(argc > 0)
	{
		switch(argv[0][0])
		{
			case 's': break;
			case 'c': type = VFPU_PRINT_COL;
					  break;
			case 'r': type = VFPU_PRINT_ROW;
					  break;
			case 'm': type = VFPU_PRINT_MATRIX;
					  break;
			case 'e': type = VFPU_PRINT_TRANS;
					  break;
			default: printf("Unknown format code '%c'\n", argv[0][0]);
					 return CMD_ERROR;
		}
	}

	if(argc > 1)
	{
		ex = atoi(argv[1]);
	}


	exceptionVfpuPrint(ex, type);

	return CMD_OK;
}

static int exresume_cmd(int argc, char **argv)
{
	if(argc > 0)
	{
		u32 addr;

		if(memDecode(argv[0], &addr))
		{
			u32 *epc;

			epc = exceptionGetReg("epc");
			if(epc != NULL)
			{
				*epc = addr;
			}
			else
			{
				printf("Could not get EPC register\n");
			}
		}
		else
		{
			return CMD_ERROR;
		}
	}

	exceptionResume();

	return CMD_OK;
}

static int setreg_cmd(int argc, char **argv)
{
	u32 addr;
	u32 *reg;

	if(memDecode(argv[0], &addr))
	{
		if(argv[0][0] != '$')
		{
			printf("Error register must start with a $\n");
			return CMD_ERROR;
		}

		reg = exceptionGetReg(&argv[0][1]);
		if(reg == NULL)
		{
			printf("Error could not find register %s\n", argv[0]);
			return CMD_ERROR;
		}

		*reg = addr;
	}

	return CMD_OK;
}

static int hwena_cmd(int argc, char **argv)
{
	if(argc > 0)
	{
		if(strcmp(argv[0], "on") == 0)
		{
			debugEnableHW();
		}
		else if(strcmp(argv[0], "off") == 0)
		{
			debugDisableHW();
		}
		else
		{
			return CMD_ERROR;
		}
	}
	else
	{
		printf("Debug HW: %s\n", debugHWEnabled() ? "on" : "off" );
	}

	return CMD_OK;
}

static int hwregs_cmd(int argc, char **argv)
{
	if(argc == 0)
	{
		debugPrintHWRegs();
	}
	else
	{
		debugSetHWRegs(argc, argv);
	}

	return CMD_OK;
}

static int bpset_cmd(int argc, char **argv)
{
	u32 addr;
	int ret = CMD_ERROR;

	if(memDecode(argv[0], &addr))
	{
		int size_left;

		addr &= ~3;
		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_WORD | MEM_ATTRIB_EXEC);
		if(size_left >= sizeof(u32))
		{
			debugSetBP(addr);
			ret = CMD_OK;
		}
		else
		{
			printf("Error, invalidate memory address for breakpoint\n");
		}
	}

	return ret;
}

static int bpprint_cmd(int argc, char **argv)
{
	debugPrintBPS();

	return CMD_OK;
}

static int step_cmd(int argc, char **argv)
{
	debugStep(0);

	return CMD_OK;
}

static int skip_cmd(int argc, char **argv)
{
	debugStep(1);

	return CMD_OK;
}

static int symload_cmd(int argc, char **argv)
{
	char source[MAXPATHLEN];

	if( !handlepath(g_context.currdir, argv[0], source, TYPE_FILE, 1) )
	{
		return CMD_ERROR;
	}

	if(!symbolLoadSymbols(source))
	{
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int symlist_cmd(int argc, char **argv)
{
	symbolPrintLoadList();

	return CMD_OK;
}

static int symprint_cmd(int argc, char **argv)
{
	symbolPrintSymbols(argv[0]);

	return CMD_OK;
}

static int symbyaddr_cmd(int argc, char **argv)
{
	u32 addr;
	int ret = CMD_ERROR;

	if(memDecode(argv[0], &addr))
	{
		const struct SymfileEntry *pEntry;
		unsigned int baseaddr;

		pEntry = symbolFindByAddress(addr, &baseaddr);
		if(pEntry)
		{
			if((baseaddr + pEntry->addr) < addr)
			{
				printf("%s+0x%x\n", pEntry->name, addr - (baseaddr + pEntry->addr));
			}
			else
			{
				printf("%s\n", pEntry->name);
			}

			ret = CMD_OK;
		}
		else
		{
			printf("Error could not find symbol at address 0x%08X\n", addr);
		}
	}

	return ret;
}

static int symbyname_cmd(int argc, char **argv)
{
	u32 addr;

	addr = symbolFindByName(argv[0]);
	if(addr > 0)
	{
		printf("%s = 0x%08X\n", argv[0], addr);
	}
	else
	{
		printf("Could not find symbol %s\n", argv[0]);
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int version_cmd(int argc, char **argv)
{
	printf("PSPLink Version %s\n", PSPLINK_VERSION);

	return CMD_OK;
}

static int pspver_cmd(int argc, char **argv)
{
	unsigned int ver;

	ver = sceKernelDevkitVersion();
	printf("Version: %d.%d\n", (ver >> 24) & 0xFF, (ver >> 16) & 0xFF);

	return CMD_OK;
}

static int wifi_cmd(int argc, char **argv)
{
	if(g_context.wifi == 0)
	{
		int ap = 1;
		if(argc > 0)
		{
			ap = atoi(argv[1]);
		}

		load_wifi(g_context.bootpath, ap);
	}
	else
	{
		printf("Wifi already enabled: ap %d\n", g_context.wifi);
	}

	return CMD_OK;
}

static int wifishell_cmd(int argc, char **argv)
{
	if(g_context.wifi == 0)
	{
		int ap = 1;
		if(argc > 0)
		{
			ap = atoi(argv[1]);
		}

		load_wifi(g_context.bootpath, ap);
	}

	load_wifishell(g_context.bootpath);

	return CMD_OK;
}

static int config_cmd(int argc, char **argv)
{
	configPrint(g_context.bootpath);

	return CMD_OK;
}

static int confset_cmd(int argc, char **argv)
{
	if(argc > 1)
	{
		configChange(g_context.bootpath, argv[0], argv[1], CONFIG_MODE_ADD);
	}
	else
	{
		configChange(g_context.bootpath, argv[0], "", CONFIG_MODE_ADD);
	}

	return CMD_OK;
}

static int confdel_cmd(int argc, char **argv)
{
	configChange(g_context.bootpath, argv[0], "", CONFIG_MODE_DEL);

	return CMD_OK;
}

static int power_cmd(int argc, char **argv)
{
	int batteryLifeTime = 0;
	char fbuf[128];

	printf("External Power: %s\n", scePowerIsPowerOnline()? "yes" : "no ");
	printf("%-14s: %s\n", "Battery", scePowerIsBatteryExist()? "present" : "absent ");

	if (scePowerIsBatteryExist()) {
	    printf("%-14s: %s\n", "Low Charge", scePowerIsLowBattery()? "yes" : "no ");
	    printf("%-14s: %s\n", "Charging", scePowerIsBatteryCharging()? "yes" : "no ");
	    batteryLifeTime = scePowerGetBatteryLifeTime();
	    printf("%-14s: %d%% (%02dh%02dm)     \n", "Charge",
		   scePowerGetBatteryLifePercent(), batteryLifeTime/60, batteryLifeTime-(batteryLifeTime/60*60));
		f_cvt((float) scePowerGetBatteryVolt() / 1000.0, fbuf, sizeof(fbuf), 3, MODE_GENERIC);
	    printf("%-14s: %sV\n", "Volts", fbuf);
	    printf("%-14s: %d deg C\n", "Battery Temp", scePowerGetBatteryTemp());
	} else
	    printf("Battery stats unavailable\n");

	printf("%-14s: %d MHz\n", "CPU Speed", scePowerGetCpuClockFrequency());
	printf("%-14s: %d MHz\n", "Bus Speed", scePowerGetBusClockFrequency());

	return CMD_OK;
}

static int poweroff_cmd(int argc, char **argv)
{
	scePowerRequestStandby();

	return CMD_OK;
}

static int clock_cmd(int argc, char **argv)
{
	u32 val1, val2, val3;

	if((strtoint(argv[0], &val1)) && (strtoint(argv[1], &val2)) && (strtoint(argv[2], &val3)))
	{
		(void) scePowerSetClockFrequency(val1, val2, val3);
	}
	else
	{
		printf("Invalid clock values\n");
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int profmode_cmd(int argc, char **argv)
{
	u32 *debug;
	const char *mode;

	debug = get_debug_register();
	if(debug)
	{
		if(argc > 0)
		{
			switch(argv[0][0])
			{
				case 't': 
						*debug &= DEBUG_REG_PROFILER_MASK;
						*debug |= DEBUG_REG_THREAD_PROFILER;
						break;
				case 'g': 
						*debug &= DEBUG_REG_PROFILER_MASK;
						*debug |= DEBUG_REG_GLOBAL_PROFILER;
						break;
				case 'o': 
						*debug &= DEBUG_REG_PROFILER_MASK;
						break;
				default: printf("Invalid profiler mode '%s'\n", argv[0]);
						 return CMD_ERROR;
			};
			printf("Profiler mode set, you must now reset psplink\n");
		}
		else
		{
			if((*debug & DEBUG_REG_THREAD_PROFILER) == DEBUG_REG_THREAD_PROFILER)
			{
				mode = "Thread";
			}
			else if((*debug & DEBUG_REG_GLOBAL_PROFILER) == DEBUG_REG_GLOBAL_PROFILER)
			{
				mode = "Global";
			}
			else
			{
				mode = "Off";
			}

			printf("Profiler Mode: %s\n", mode);
		}
	}

	
	return CMD_OK;
}

static int debugreg_cmd(int argc, char **argv)
{
	u32 *debug;

	debug = get_debug_register();
	if(debug)
	{
		if(argc > 0)
		{
			u32 val;

			if(strtoint(argv[0], &val))
			{
				*debug = val;
			}
			else
			{
				printf("Invalid debug reg value '%s'\n", argv[0]);
				return CMD_ERROR;
			}
		}
		else
		{
			printf("Debug Register: 0x%08X\n", *debug);
		}
	}

	return CMD_OK;
}

static int tty_cmd(int argc, char **argv)
{
	g_ttymode = 1;
	return CMD_OK;
}

static int tonid_cmd(int argc, char **argv)
{
	printf("Name: %s, Nid: 0x%08X\n", argv[0], libsNameToNid(argv[0]));

	return CMD_OK;
}

static int exit_cmd(int argc, char **argv)
{
	return CMD_EXITSHELL;
}

static int help_cmd(int argc, char **argv);

static int custom_cmd(int argc, char **argv);

/* Structure to hold a single command entry */
struct sh_command 
{
	const char *name;		/* Normal name of the command */
	const char *syn;		/* Synonym of the command */
	int (*func)(int argc, char **argv);		/* Pointer to the command function */
	int min_args;
	const char *desc;		/* Textual description */
	const char *help;		/* Command usage */
};

/* Define the list of commands */
const struct sh_command commands[] = {
	{ "thread", NULL, NULL, 0, "Commands to manipulate threads", NULL },
	{ "thlist", "tl", thlist_cmd, 0, "List the threads in the system", "[v]" },
	{ "thsllist", NULL, thsllist_cmd, 0, "List the sleeping threads in the system", "[v]" },
	{ "thdelist", NULL, thdelist_cmd, 0, "List the delayed threads in the system", "[v]" },
	{ "thsulist", NULL, thsulist_cmd, 0, "List the suspended threads in the system", "[v]" },
	{ "thdolist", NULL, thdolist_cmd, 0, "List the dormant threads in the system", "[v]" },
	{ "thinfo", "ti", thinfo_cmd, 1, "Print info about a thread", "uid|@name" },
	{ "thsusp", "ts", thsusp_cmd, 1, "Suspend a thread", "uid|@name" },
	{ "thspuser", NULL, thspuser_cmd, 0, "Suspend all user threads", "" },
	{ "thresm", "tr", thresm_cmd, 1, "Resume a thread", "uid|@name"},
	{ "thwake", "tw", thwake_cmd, 1, "Wakeup a thread", "uid|@name"},
	{ "thterm", "tt", thterm_cmd, 1, "Terminate a thread", "uid|@name"},
	{ "thdel", "td", thdel_cmd, 1, "Delete a thread", "uid|@name"},
	{ "thtdel", "tx", thtdel_cmd, 1, "Terminate and delete a thread", "uid|@name" },
	{ "thctx",  "tt", thctx_cmd, 1, "Find and print the full thread context", "uid|@name" },
	{ "thpri",  "tp", thpri_cmd, 2, "Change a threads current priority", "uid|@name pri" },
	{ "evlist", "el", evlist_cmd, 0, "List the event flags in the system", "[v]" },
	{ "evinfo", "ei", evinfo_cmd, 1, "Print info about an event flag", "uid|@name" },
	{ "smlist", "sl", smlist_cmd, 0, "List the semaphores in the system", "[v]" },
	{ "sminfo", "si", sminfo_cmd, 1, "Print info about a semaphore", "uid|@name" },
	{ "mxlist", "xl", mxlist_cmd, 0, "List the message boxes in the system", "[v]" },
	{ "mxinfo", "xi", mxinfo_cmd, 1, "Print info about a message box", "uid|@name" },
	{ "cblist", "cl", cblist_cmd, 0, "List the callbacks in the system", "[v]" },
	{ "cbinfo", "ci", cbinfo_cmd, 1, "Print info about a callback", "uid|@name" },
	{ "vtlist", "zl", vtlist_cmd, 0, "List the virtual timers in the system", "[v]" },
	{ "vtinfo", "zi", vtinfo_cmd, 1, "Print info about a virtual timer", "uid|@name" },
	{ "vpllist","vl", vpllist_cmd, 0, "List the variable pools in the system", "[v]" },
	{ "vplinfo","vi", vplinfo_cmd, 1, "Print info about a variable pool", "uid|@name" },
	{ "fpllist","fl", fpllist_cmd, 0, "List the fixed pools in the system", "[v]" },
	{ "fplinfo","fi", fplinfo_cmd, 1, "Print info about a fixed pool", "uid|@name" },
	{ "mpplist","pl", mpplist_cmd, 0, "List the message pipes in the system", "[v]" },
	{ "mppinfo","pi", mppinfo_cmd, 1, "Print info about a message pipe", "uid|@name" },
	{ "thevlist","tel", thevlist_cmd, 0, "List the thread event handlers in the system", "[v]" },
	{ "thevinfo","tei", thevinfo_cmd, 1, "Print info about a thread event handler", "uid|@name" },
	{ "thmon", "tm", thmon_cmd, 1, "Monitor thread events", "u|k|a [csed]" },
	{ "thmonoff", NULL, thmonoff_cmd, 0, "Disable the thread monitor", "" },
	{ "sysstat", NULL, sysstat_cmd, 0, "Print the system status", "" },
	
	{ "module", NULL, NULL, 0, "Commands to handle modules", NULL },
	{ "modlist","ml", modlist_cmd, 0, "List the currently loaded modules", "[v]" },
	{ "modinfo","mi", modinfo_cmd, 1, "Print info about a module", "uid|@name" },
	{ "modstop","ms", modstop_cmd, 1, "Stop a running module", "uid|@name" },
	{ "modunld","mu", modunld_cmd, 1, "Unload a module (must be stopped)", "uid|@name" },
	{ "modstun","mn", modstun_cmd, 1, "Stop and unload a module", "uid|@name" },
	{ "modload","md", modload_cmd, 1, "Load a module", "path" },
	{ "modstart","mt", modstart_cmd, 1, "Start a module", "uid|@name [args]" },
	{ "modexec","me", modexec_cmd, 1, "LoadExec a module", "[@key] path [args]" },
	{ "modaddr","ma", modaddr_cmd, 1, "Display info about the module at a specified address", "addr" },
	{ "exec", "e", exec_cmd, 0, "Execute a new program (under psplink)", "[path] [args]" },
	{ "ldstart","ld", ldstart_cmd, 1, "Load and start a module", "path [args]" },
	{ "kill", "k", kill_cmd, 1, "Kill a module and all it's threads", "uid|@name" },
	{ "debug", "d", debug_cmd, 1, "Start a module under GDB", "program.elf [args]" },
	{ "modexp", "mp", modexp_cmd, 1, "List the exports from a module", "uid|@name" },
	{ "modimp", NULL, modimp_cmd, 1, "List the imports in a module", "uid|@name" },
	{ "modfindx", "mfx", modfindx_cmd, 3, "Find a module's export address", "uid|@name library nid|@name" },
	{ "apihook", NULL, apihook_cmd, 4, "Hook a user mode API call", "uid|@name library nid|@name ret [param]" },
	{ "apihooks", NULL, apihooks_cmd, 4, "Hook a user mode API call with sleep", "uid|@name library nid|@name ret [param]" },
	{ "apihp", NULL, apihp_cmd, 0, "Print the user mode API hooks", "" },
	{ "apihd", NULL, apihd_cmd, 1, "Delete an user mode API hook", "id" },
	
	{ "memory", NULL, NULL, 0, "Commands to manipulate memory", NULL },
	{ "meminfo", "mf", meminfo_cmd, 0, "Print free memory info", "[partitionid]" },
	{ "memreg",  "mr", memreg_cmd, 0, "Print available memory regions (for other commands)", "" },
	{ "memdump", "dm", memdump_cmd, 0, "Dump memory to screen", "[addr|-] [b|h|w]" },
	{ "memblocks", "mk", memblocks_cmd, 0, "Dump the sysmem block table", "[f|t]" },
	{ "savemem", "sm", savemem_cmd, 3, "Save memory to a file", "addr size path" },
	{ "loadmem", "lm", loadmem_cmd, 2, "Load memory from a file", "addr path [maxsize]" },
	{ "pokew",   "pw", pokew_cmd, 2, "Poke words into memory", "addr val1 [val2..valN]"},
	{ "pokeh",   "pw", pokeh_cmd, 2, "Poke half words into memory", "addr val1 [val2..valN]"},
	{ "pokeb",   "pw", pokeb_cmd, 2, "Poke bytes into memory", "addr val1 [val2..valN]"},
	{ "peekw",   "kw", peekw_cmd, 1, "Peek the word at address", "addr [o|b|x|f]"},
	{ "peekh",   "kh", peekh_cmd, 1, "Peek the half word at address", "addr [o|b|x]"},
	{ "peekb",   "kb", peekb_cmd, 1, "Peek the byte at address", "addr [o|b|x]"},
	{ "fillw",   "fw", fillw_cmd, 3, "Fill a block of memory with a word value", "addr size val"},
	{ "fillh",   "fh", fillh_cmd, 3, "Fill a block of memory with a half value", "addr size val"},
	{ "fillb",   "fb", fillb_cmd, 3, "Fill a block of memory with a byte value", "addr size val"},
	{ "copymem", "cm", copymem_cmd, 3, "Copy a block of memory", "srcaddr destaddr size"},
	{ "findstr", "ns", findstr_cmd, 3, "Find an ASCII string", "addr size str"},
	{ "findhex", "nx", findhex_cmd, 3, "Find an hexstring string", "addr size hexstr [mask]"},
	{ "findw",   "nw", findw_cmd, 3, "Find a list of words", "addr size val1 [val2..valN]"},
	{ "findh",   "nh", findh_cmd, 3, "Find a list of half words", "addr size val1 [val2..valN]"},
	{ "dcache",  "dc", dcache_cmd, 1, "Perform a data cache operation", "w|i|wi [addr size]"},
	{ "icache",  "ic", icache_cmd, 0, "Perform an instruction cache operation", "[addr size]"},
	{ "disasm",  "di", disasm_cmd, 1, "Disassemble instructions", "address [count]"},
	{ "disopts", NULL, disopts_cmd, 0, "Print the current disassembler options", ""},
	{ "disset", NULL, disset_cmd, 1, "Set some disassembler options", "options"},
	{ "disclear", NULL, disclear_cmd, 1, "Clear some disassembler options", "options"},
	{ "memprot", NULL, memprot_cmd, 1, "Set memory protection on or off", "on|off" },
	
	{ "fileio", NULL, NULL, 0, "Commands to handle file io", NULL},
	{ "ls",  "dir", ls_cmd,    0, "List the files in a directory", "[path1..pathN]"},
	{ "chdir", "cd", chdir_cmd, 1, "Change the current directory", "path"},
	{ "cp",  "copy", cp_cmd, 2, "Copy a file", "source destination"},
	{ "mkdir", NULL, mkdir_cmd, 1, "Make a Directory", "dir"},
	{ "rm", "del", rm_cmd, 1, "Removes a File", "file"},
	{ "rmdir", "rd", rmdir_cmd, 1, "Removes a Directory", "dir"},
	{ "rename", "ren", rename_cmd, 2, "Renames a File", "src dst"},
	{ "remap", NULL, remap_cmd, 2, "Remaps a device to another", "devfrom: devto:"},
	{ "pwd",   NULL, pwd_cmd, 0, "Print the current working directory", ""},

	{ "debugger", NULL, NULL, 0, "Debug commands", NULL},
	{ "exprint", "ep", exprint_cmd, 0, "Print the current exception info", "[ex]"},
	{ "exlist",  "el", exlist_cmd, 0, "List the exception contexts", "" },
	{ "exctx",   "ec", exctx_cmd, 1, "Set the current exception context", "ex" },
	{ "exresume", "c", exresume_cmd, 0, "Resume from the exception", "[addr]"},
	{ "exprfpu", "ef", exprfpu_cmd, 0, "Print the current FPU registers", "[ex]"},
	{ "exprvfpu", "ev", exprvfpu_cmd, 0, "Print the current VFPU registers", "[s|c|r|m|e] [ex]"},
	{ "setreg", "str", setreg_cmd, 2, "Set the value of an exception register", "$reg value"},
	{ "hwena",  NULL, hwena_cmd, 0, "Enable or disable the HW debugger", "[on|off]" },
	{ "hwregs", NULL, hwregs_cmd, 0, "Print or change the current HW breakpoint setup (v1.5 only)", "[reg=val]..." },
	{ "bpset", "bp", bpset_cmd, 1, "Set a break point", "addr"},
	{ "bpprint", "bt", bpprint_cmd, 0, "Print the current breakpoints", ""},
	{ "step", "s", step_cmd, 0, "Step the next instruction", ""},
	{ "skip", "k", skip_cmd, 0, "Skip the next instruction (i.e. jump over jals)", ""},
	{ "symload", "syl", symload_cmd, 1, "Load a symbol file", "file.sym"},
	{ "symlist", "syt", symlist_cmd, 0, "List the loaded symbols", ""},
	{ "symprint", "syp", symprint_cmd, 1, "Print the symbols for a module", "modname"},
	{ "symbyaddr", "sya", symbyaddr_cmd, 1, "Print the symbol at the specified address", "addr"},
	{ "symbyname", "syn", symbyname_cmd, 1, "Print the specified symbol address", "module:symname"},

	{ "misc", NULL, NULL, 0, "Miscellaneous commands (e.g. USB, exit)", NULL},
	{ "usbmon", "umn", usbmasson_cmd, 0, "Enable USB mass storage device", ""},
	{ "usbmoff", "umf", usbmassoff_cmd, 0, "Disable USB mass storage device", ""},
	{ "usbhon", "uhn", usbhoston_cmd, 0, "Enable USB hostfs device", ""},
	{ "usbhoff", "uhf", usbhostoff_cmd, 0, "Disable USB hostfs device", ""},
	{ "usbstat", "us", usbstat_cmd, 0, "Display the status of the USB connection", ""},
    { "uidlist","ul", uidlist_cmd, 0, "List the system UIDS", "[root]"},
	{ "uidinfo", "ui", uidinfo_cmd, 1, "Print info about a UID", "uid|@name [parent]" },
	{ "cop0", "c0", cop0_cmd, 0, "Print the cop0 registers", ""},
	{ "exit", "quit", exit_cmd, 0, "Exit the shell", ""},
	{ "set", NULL, set_cmd, 0, "Set a shell variable", "[var=value]"},
	{ "scrshot", "ss", scrshot_cmd, 1, "Take a screen shot", "file"},
	{ "run",  NULL, run_cmd, 1, "Run a shell script", "file [args]"},
	{ "calc", NULL, calc_cmd, 1, "Do a simple address calculation", "addr [d|o|x]"},
	{ "reset", "r", reset_cmd, 0, "Reset", "[key]"},
	{ "wifi", NULL, wifi_cmd, 0, "Enable WIFI with a specified AP config", "[ap]"},
	{ "wifishell", NULL, wifishell_cmd, 0, "Enable WIFI Shell with a specified AP config", "[ap]"},
	{ "ver", "v", version_cmd, 0, "Print version of psplink", ""},
	{ "pspver", NULL, pspver_cmd, 0, "Print the version of PSP", ""},
	{ "config", NULL, config_cmd, 0, "Print the configuration file settings", ""},
	{ "confset", NULL, confset_cmd, 1, "Set a configuration value", "name [value]"},
	{ "confdel", NULL, confdel_cmd, 1, "Delete a configuration value", "name"},
	{ "power", NULL, power_cmd, 0, "Print power information", ""},
	{ "poweroff", NULL, poweroff_cmd, 0, "Power off the PSP", ""},
	{ "clock", NULL, clock_cmd, 3, "Set the clock frequencies", "cpu ram bus" },
	{ "tty", NULL, tty_cmd, 0, "Enter TTY mode. All input goes to stdin", ""},
	{ "tonid", NULL, tonid_cmd, 1, "Calculate the NID from a name", "name" },
	{ "profmode", NULL, profmode_cmd, 0, "Set or display the current profiler mode", "[t|g|o]" },
	{ "debugreg", NULL, debugreg_cmd, 0, "Set or display the current debug register", "[val]" },
	{ "help", "?", help_cmd, 0, "Help (Obviously)", "[command|category]"},
	{ "custom", "cst", custom_cmd, 1, NULL, NULL},
	{ NULL, NULL, NULL, 0, NULL, NULL}
};

/* Find a command from the command list */
static const struct sh_command* find_command(const char *cmd)
{
	const struct sh_command* found_cmd = NULL;
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
	char *ext;

	if(g_ttymode)
	{
		if((command[0] == '~') && (command[1] == '.'))
		{
			g_ttymode = 0;
		}
		else
		{
			ttyAddInputData(command, strlen(command));
		}
	}
	else
	{
		if(parse_args(command, outbuf, &argc, argv, 16) == 0)
		{
			printf("Error parsing command\n");
			return CMD_ERROR;
		}

		if((argc > 0) && (argv[0][0] != '#'))
		{
			const struct sh_command *found_cmd;

			/* See if the command contains a '.', if so this cannot be a command, try and execute it direct */
			cmd = argv[0];
			ext = strrchr(cmd, '.');
			if(ext)
			{
				char path[MAXPATHLEN];

				/* Not a relative path, try and find it in our path */
				if(strchr(cmd, '/') == NULL)
				{
					const char *pathvar;

					pathvar = find_shell_var("path");

					if(findinpath(cmd, path, pathvar) == 0)
					{
						printf("Could not find %s in the path\n", cmd);
						return CMD_ERROR;
					}
					/* Otherwise assign to argv[0] */
					argv[0] = path;
				}

				if((strcmp(ext, ".sh") == 0) || (strcmp(ext, ".SH") == 0))
				{
					ret = run_cmd(argc, argv);
				}
				else
				{
					ret = ldstart_cmd(argc, argv);
				}
			}
			else
			{
				/* Check for a completion function */
				found_cmd = find_command(cmd);
				if((found_cmd) && (found_cmd->func))
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
		}
	}

	return ret;
}

static int shellParseThread(SceSize args, void *argp)
{
	int error;
	void *data;
	CommandMsg *msg;

	if((args > 0) && (argp))
	{
		/* Run startup script */
		scriptRun(argp, 0, NULL, NULL, 0);
	}

	while(1)
	{
		error = sceKernelReceiveMbx(g_command_msg, &data, NULL);
		if(error < 0)
		{
			printf("Error in receiving message 0x%08X\n", error);
			sceKernelExitDeleteThread(0);
		}

		msg = (CommandMsg *) data;
		msg->res = shellParse(msg->command);
		sceKernelSetEventFlag(g_command_event, COMMAND_EVENT_DONE);
	}

	return 0;
}

int psplinkParseCommand(char *command)
{
	u32 k1;
	int ret;
	CommandMsg msg;
	SceUInt timeout = (10*1000*1000);

	k1 = psplinkSetK1(0);

	ret = sceKernelWaitSema(g_cli_sema, 1, &timeout);
	if(ret < 0)
	{
		printf("Error, could not wait on cli sema 0x%08X\n", ret);
		return 1;
	}

	msg.command = command;
	msg.res = 0;
	ret = sceKernelSendMbx(g_command_msg, &msg);
	if(ret >= 0)
	{
		/* Wait 60 seconds for completion */
		SceUInt timeout = (60*1000*1000);
		u32 result;
		ret = sceKernelWaitEventFlag(g_command_event, COMMAND_EVENT_DONE, 0x21, &result, &timeout);
		if(ret >= 0)
		{
			ret = msg.res;
		}
		else
		{
			printf("Error, command did not complete 0x%08X\n", ret);
			ret = CMD_EXITSHELL;
		}
	}

	sceKernelSignalSema(g_cli_sema, 1);
	psplinkSetK1(k1);

	return ret;
}

#ifndef USB_ONLY

/* Process command line */
static int process_cli()
{
	int ret;

	putchar(13);
	putchar(10);

	g_cli[g_cli_pos] = 0;
	g_cli_pos = 0;
	memcpy(&g_lastcli[g_lastcli_pos][0], g_cli, CLI_MAX);
	g_lastcli_pos = (g_lastcli_pos + 1) % CLI_HISTSIZE;
	g_currcli_pos = g_lastcli_pos;

	ret = psplinkParseCommand(g_cli);
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

int shellProcessChar(int ch)
{
	int exit_shell = 0;

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
					  putchar(8);
					  putchar(' ');
					  putchar(8);
				  }
				  break;
		case 9  : break; // Ignore tab
		case 13 :		 // Enter key 
		case 10 : if(process_cli() == CMD_EXITSHELL) 
				  {
					  exit_shell = 1;
				  }
				  break;
				  /* TODO: CTRL + P and CTRL + N */
		case 11 : /* CTRL + K */
				  debugStep(1);
				  break;
		case 18 : /* CTRL + R */
				  psplinkReset();
				  break;
		case 19 : /* CTRL + S */
				  debugStep(0);
				  break;
		case 27 : /* Escape character */
				  cli_handle_escape();
				  break;
		default : if((g_cli_pos < (CLI_MAX - 1)) && (ch >= 32))
				  {
					  g_cli[g_cli_pos++] = ch;
					  g_cli[g_cli_pos] = 0;
					  putchar(ch);
				  }
				  break;
	}

	return exit_shell;
}

/* Main shell function */
void shellStart(void)
{		
	int exit_shell = 0;

	print_prompt();

	if(g_context.pcterm)
	{
		char cli[1024];
		int  pos = 0;

		while(!exit_shell)
		{
			int ch;
			int ret;

			ch = g_readchar();
			switch(ch)
			{
				case 10:
				case 13: cli[pos] = 0;
						 ret = psplinkParseCommand(cli);
						 if(ret != CMD_EXITSHELL)
						 {
							print_prompt();
							pos = 0;
						 }
						 else
						 {
							 exit_shell = 1;
						 }

						 break;
				/* TODO: CTRL + P and CTRL + N */
				case 11 : /* CTRL + K */
					  debugStep(1);
					  break;
				case 18 : /* CTRL + R */
					  psplinkReset();
					  break;
				case 19 : /* CTRL + S */
					  debugStep(0);
					  break;
				default: if(ch >= 32)
						 {
							 if(pos < (sizeof(cli)-1))
							 {
								 cli[pos++] = ch;
							 }
						 }
						 break;
			};
		}
	}
	else
	{
		g_cli_pos = 0;
		g_cli_size = 0;
		memset(g_cli, 0, CLI_MAX);

		while(!exit_shell) {
			int ch;

			ch = g_readchar();

			exit_shell = shellProcessChar(ch);
		}
	}
}

#endif

/* Help command */
static int help_cmd(int argc, char **argv)
{
	int cmd_loop;

	if(argc < 1)
	{
		printf("Command Categories\n\n");
		for(cmd_loop = 0; commands[cmd_loop].name; cmd_loop++)
		{
			if(commands[cmd_loop].func == NULL)
			{
				printf("%-10s - %s\n", commands[cmd_loop].name, commands[cmd_loop].desc);
			}
		}
		printf("\nType 'help category' for more information\n");
	}
	else
	{
		const struct sh_command* found_cmd;

		found_cmd = find_command(argv[0]);
		if((found_cmd != NULL) && (found_cmd->desc))
		{
			if(found_cmd->func == NULL)
			{
				/* Print the commands listed under the separator */
				printf("Category %s\n\n", found_cmd->name);
				for(cmd_loop = 1; found_cmd[cmd_loop].name && found_cmd[cmd_loop].func != NULL; cmd_loop++)
				{
					if(found_cmd[cmd_loop].desc)
					{
						printf("%-10s - %s\n", found_cmd[cmd_loop].name, found_cmd[cmd_loop].desc);
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
				printf("Usage: %s %s\n", found_cmd->name, found_cmd->help);
			}
		}
		else
		{
			printf("Unknown command %s, type help for information\n", argv[0]);
		}
	}

	return CMD_OK;
}

static int custom_cmd(int argc, char **argv)
{
	int retval = CMD_OK;
	int cmdnum = atoi(argv[0]);
	char cmd[64];
	
	cmd[0] = 0;

	switch(cmdnum) 
	{
	case 0:
		strcpy(cmd, g_context.conscrosscmd);
		break;
	case 1:
		strcpy(cmd, g_context.conssquarecmd);
		break;
	case 2:
		strcpy(cmd, g_context.constrianglecmd);
		break;
	case 3:
		strcpy(cmd, g_context.conscirclecmd);
		break;
	case 4:
		strcpy(cmd, g_context.consselectcmd);
		break;
	case 5:
		strcpy(cmd, g_context.consstartcmd);
		break;
	case 6:
		strcpy(cmd, g_context.consdowncmd);
		break;
	case 7:
		strcpy(cmd, g_context.consleftcmd);
		break;
	case 8:
		strcpy(cmd, g_context.consupcmd);
		break;
	case 9:
		strcpy(cmd, g_context.consrightcmd);
		break;
	default:
		printf("Error: Illegal custom command\n");
		break;
	}

	if(strlen(cmd) > 0)
	{
		printf("%s\n", cmd);
		retval = shellParse(cmd);
		print_prompt();
	}
	return retval;
}

int shellInit(const char *cliprompt, const char *path, const char *init_dir, const char *startsh)
{
	int ret;

	if(strlen(cliprompt) > 0)
	{
		set_shell_var("prompt", cliprompt);
	}

	set_shell_var("path", path);

	strcpy(g_context.currdir, init_dir);

	g_command_thid = sceKernelCreateThread("PspLinkParse", shellParseThread, 9, 0x10000, 0, NULL);
	if(g_command_thid < 0)
	{
		printf("Error, couldn't create thread for parsing 0x%08X\n", g_command_thid);
		return -1;
	}

	g_command_msg = sceKernelCreateMbx("PspLinkCmdMbx", 0, 0);
	if(g_command_msg < 0)
	{
		printf("Error, couldn't create message box 0x%08X\n", g_command_msg);
		return -1;
	}

	g_cli_sema = sceKernelCreateSema("PspLinkCliSema", 0, 1, 1, NULL);
	if(g_cli_sema < 0)
	{
		printf("Error, couldn't create cli semaphore 0x%08X\n", g_cli_sema);
		return -1;
	}

	g_command_event = sceKernelCreateEventFlag("PspLinkCmdEvent", 0, 0, NULL);
	if(g_command_event < 0)
	{
		printf("Error, couldn't create command event 0x%08X\n", g_command_event);
		return -1;
	}

	if(strlen(startsh) > 0)
	{
		ret = sceKernelStartThread(g_command_thid, strlen(startsh)+1, (void*) startsh);
	}
	else
	{
		ret = sceKernelStartThread(g_command_thid, 0, NULL);
	}

	if(ret < 0)
	{
		printf("Error, couldn't start command thread 0x%08X\n", ret);
		return -1;
	}

	return 0;
}
