/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK kernel module main code.
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
#include <pspusb.h>
#include <pspusbstor.h>
#include "memoryUID.h"
#include "psplink.h"

PSP_MODULE_INFO("PSPLINK", 0x1000, 1, 1);

#define WELCOME_MESSAGE "PSPLINK Initialised\n"

#define SHELL_PROMPT	"psplink> "

#define BOOTLOADER_NAME "PSPLINKLOADER"

/* Maximum command line */
#define CLI_MAX			128
/* Maximum history */
#define CLI_HISTSIZE	8
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
/* The filename of the bootstrap */
static const char *g_bootfile = NULL;
/* The thread ID of the loader */
static int g_loaderthid = 0;
/* The program to execute */
//static const char *g_execfile = NULL;
static char g_execfile[256];
/* Inidicates whether a file has already been executed */
static int  g_inexec = 0;

extern int g_debuggermode;
void set_swbp(u32 addr);

/* Global functions which are setup to point to the correct function
   for the firmware */

static int (*g_QueryModuleInfo)(SceUID modid, SceKernelModuleInfo *info) = NULL;
static int (*g_GetModuleIdList)(SceUID *readbuf, int readbufsize, int *idcount) = NULL;

static SceUID g_eventflag = -1;

int intr_handler(void *arg)
{
	u32 stat;

	stat = _lw(0xBE500040);
	_sw(stat, 0xBE500044);

	sceKernelDisableIntr(PSP_HPREMOTE_INT);

	sceKernelSetEventFlag(g_eventflag, EVENT_SIO);

	return -1;
}

/* Make the character upper case */
static char upcase(char ch)
{
	if((ch >= 'a') && (ch <= 'z'))
	{
		ch ^= (1 << 5);
	}

	return ch;
}

int stop_usb(void);
void sceKernelDcacheWBinvAll(void);
void sceKernelIcacheClearAll(void);

static int exit_cmd(void);
static int help_cmd(void);
static int thlist_cmd(void);
static int thinfo_cmd(void);
static int evlist_cmd(void);
static int evinfo_cmd(void);
static int smlist_cmd(void);
static int sminfo_cmd(void);
static int uidlist_cmd(void);
static int modlist_cmd(void);
static int modinfo_cmd(void);
static int modstop_cmd(void);
static int modunld_cmd(void);
static int modstart_cmd(void);
static int modload_cmd(void);
static int modexec_cmd(void);
static int ldstart_cmd(void);
static int exec_cmd(void);
static int debug_cmd(void);
static int reset_cmd(void);

/* Return values for the commands */
#define CMD_EXITSHELL 	1
#define CMD_OK		  	0
#define CMD_ERROR		-1

/* Structure to hold a single command entry */
struct sh_command 
{
	const char *name;		/* Normal name of the command */
	const char *syn;		/* Synonym of the command */
	int (*func)(void);		/* Pointer to the command function */
	const char *desc;		/* Textual description */
	const char *help;		/* Command usage */
};

/* Define the list of commands */
struct sh_command commands[] = {
	{ "thlist", "tl", thlist_cmd, "List the threads in the system", "tl [v]" },
	{ "thinfo", "ti", thinfo_cmd, "Print info about a thread", "ti uid" },
	{ "evlist", "el", evlist_cmd, "List the event flags in the system", "el [v]" },
	{ "evinfo", "ei", evinfo_cmd, "Print info about an event flag", "ei uid" },
	{ "smlist", "sl", smlist_cmd, "List the semaphores in the system", "sl [v]" },
	{ "sminfo", "si", sminfo_cmd, "Print info about a semaphore", "si uid" },
    { "uidlist","ul", uidlist_cmd,"List the system UIDS", "ul" },
	{ "modlist","ml", modlist_cmd,"List the currently loaded modules", "ml [v]" },
	{ "modinfo","mi", modinfo_cmd,"Print info about a module", "mi uid" },
	{ "modstop","ms", modstop_cmd,"Stop a running module", "ms uid" },
	{ "modunld","mu", modunld_cmd,"Unload a module (must be stopped)", "mu uid" },
	{ "modload","md", modload_cmd,"Load a module", "md path" },
	{ "modstar","mt", modstart_cmd,"Start a module", "mt uid" },
	{ "modexec","me", modexec_cmd, "LoadExec a module", "me path" },
	{ "ldstart","ld", ldstart_cmd, "Load and start a module", "ld path" },
	{ "exec", "e", exec_cmd, "Execute a new program (under psplink)", "exec [path]" },
	{ "debug", "d", debug_cmd, "Debug an executable (need to switch to gdb)", "debug path" },
	{ "exit", "ex", exit_cmd, "Exit the shell", "exit" },
	{ "reset", "r", reset_cmd, "Reset", "r" },
	{ "help", "?", help_cmd, "Help (Obviously)", "help [command]" },
	{ NULL, NULL, NULL, NULL }
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

/* Read a character with a timeout */
static int read_char(void)
{
	int ch;
	u32 result;
	u32 timeout;

	timeout = 500000;
	ch = pspDebugSioGetchar();
	if(ch == -1)
	{
		sceKernelEnableIntr(PSP_HPREMOTE_INT);
		sceKernelWaitEventFlag(g_eventflag, EVENT_SIO, 0x21, &result, &timeout);
		ch = pspDebugSioGetchar();
	}

	return ch;
}

static int GetChar(void)
{
	int ch;
	u32 result;

	ch = pspDebugSioGetchar();
	if(ch == -1)
	{
		sceKernelEnableIntr(PSP_HPREMOTE_INT);
		sceKernelWaitEventFlag(g_eventflag, EVENT_SIO, 0x21, &result, NULL);
		ch = pspDebugSioGetchar();
	}

	return ch;
}

/* Process command line */
static int process_cli()
{
	int ret = CMD_OK;
	char *cmd;

    pspDebugSioPutchar(13);
    pspDebugSioPutchar(10);
	g_cli[g_cli_pos] = 0;
	memcpy(&g_lastcli[g_lastcli_pos][0], g_cli, CLI_MAX);
	g_lastcli_pos = (g_lastcli_pos + 1) % CLI_HISTSIZE;
	g_currcli_pos = g_lastcli_pos;

	cmd = strtok(g_cli, " ");

	if(cmd != NULL)
	{
		struct sh_command *found_cmd;

		found_cmd = find_command(cmd);
		if(found_cmd)
		{
			ret = found_cmd->func();
			if(ret == CMD_ERROR)
			{
				Kprintf("Usage: %s\n", found_cmd->help);
			}
		}
		else
		{
			Kprintf("Unknown command %s\n", cmd);
		}
	}

	g_cli_pos = 0;
	if(ret != CMD_EXITSHELL)
	{
		Kprintf(SHELL_PROMPT);
	}

	return ret;
}

/* Handle an escape sequence */
static void cli_handle_escape(void)
{
	char ch;

	ch = read_char();

	if(ch != -1)
	{
		/* Arrow keys UDRL/ABCD */
		if(ch == '[')
		{
			ch = read_char();
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

								   Kprintf("\n%s%s", SHELL_PROMPT, g_cli);
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

								   Kprintf("\n%s%s", SHELL_PROMPT, g_cli);
							   } 
						   } 
						   break;



				default: 
							Kprintf("Unknown character %d\n", ch);
						   break;
			};
		}
		else
		{
			Kprintf("Unknown character %d\n", ch);
		}
	}
}

/* Main shell function */
void shell()
{		
	int exit_shell = 0;

	Kprintf(SHELL_PROMPT);
	g_cli_pos = 0;
	g_cli_size = 0;
	memset(g_cli, 0, CLI_MAX);

	while(!exit_shell) {
		char ch;

		ch = GetChar();
		switch(ch)
		{
			case -1 : break; // No char
					  /* ^D */
			case 4  : Kprintf("\nExiting Shell\n");
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

typedef int (*threadmanprint_func)(SceUID uid, int verbose);

static int threadmanlist_cmd(enum SceKernelIdListType type, const char *name, threadmanprint_func pinfo)
{
	SceUID ids[100];
	int ret;
	int count;
	int i;
	char *arg;
	int verbose = 0;

	arg = strtok(NULL, " ");
	if((arg != NULL) && (strcmp(arg, "v") == 0))
	{
		verbose = 1;
	}

	memset(ids, 0, 100 * sizeof(SceUID));
	ret = sceKernelGetThreadmanIdList(type, ids, 100, &count);
	if(ret >= 0)
	{
		Kprintf("<%s List>\n", name);
		for(i = 0; i < count; i++)
		{
			if(pinfo(ids[i], verbose) < 0)
			{
				Kprintf("ERROR: Unknown %s %08X\n", name, ids[i]);
			}
		}
	}

	return CMD_OK;
}

static int threadmaninfo_cmd(const char *name, threadmanprint_func pinfo)
{
	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;

	suid = strtok(NULL, " \t");
	if(suid != NULL)
	{
		char *endp;
		uid = strtoul(suid, &endp, 16);
		if(*endp == 0)
		{
			if(pinfo(uid, 1) < 0)
			{
				Kprintf("ERROR: Unknown %s %08X\n", name, uid);
			}

			ret = CMD_OK;
		}
		else
		{
			Kprintf("ERROR: Invalid hex argument %s\n", suid);
		}
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
		Kprintf("UID: %08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			Kprintf("Attr: %08X - Status: %d - Entry: %08X\n", info.attr, info.status, info.entry);
			Kprintf("Stack: %08X - StackSize %08X - GP: %08X\n", (u32) info.stack, info.stackSize,
					(u32) info.gpReg);
			Kprintf("InitPri: %d - CurrPri: %d - WaitType %d\n", info.initPriority,
					info.currentPriority, info.waitType);
			Kprintf("WaitId: %08X - WakeupCount: %d - ExitStatus: %08X\n", info.waitId,
					info.wakeupCount, info.exitStatus);
			Kprintf("RunClocks: %d - IntrPrempt: %d - ThreadPrempt: %d\n", info.runClocks,
					info.intrPreemptCount, info.threadPreemptCount);
			Kprintf("ReleaseCount: %d\n", info.releaseCount);
		}
	}

	return ret;
}

static int thlist_cmd(void)
{
	return threadmanlist_cmd(SCE_KERNEL_TMID_Thread, "Thread", print_threadinfo);
}

static int thinfo_cmd(void)
{
	return threadmaninfo_cmd("Thread", print_threadinfo);
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
		Kprintf("UID: %08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			Kprintf("Attr: %08X - initPattern %08X - currPatten %08X\n", info.attr, info.initPattern, 
					info.currentPattern);
			Kprintf("NumWaitThreads: %08X\n", info.numWaitThreads);
		}
	}

	return ret;
}

static int evlist_cmd(void)
{
	return threadmanlist_cmd(SCE_KERNEL_TMID_EventFlag, "EventFlag", print_eventinfo);
}

static int evinfo_cmd(void)
{
	return threadmaninfo_cmd("EventFlag", print_eventinfo);
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
		Kprintf("UID: %08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			Kprintf("Attr: %08X - initCount: %08X - currCount: %08X\n", info.attr, info.initCount, 
					info.currentCount);
			Kprintf("maxCount: %08X - NumWaitThreads: %08X\n", info.maxCount, info.numWaitThreads);
		}
	}

	return ret;
}

static int smlist_cmd(void)
{
	return threadmanlist_cmd(SCE_KERNEL_TMID_Semaphore, "Semaphore", print_semainfo);
}

static int sminfo_cmd(void)
{
	return threadmaninfo_cmd("Semaphore", print_semainfo);
}

static int uidlist_cmd(void)
{
	printUIDList();

	return CMD_OK;
}

static int print_modinfo(SceUID uid, int verbose)
{
	SceKernelModuleInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	ret = g_QueryModuleInfo(uid, &info);
	if(ret >= 0)
	{
		Kprintf("UID: %08X Attr: %04X - Name: %s\n", uid, info.attribute, info.name);
		if(verbose)
		{
			Kprintf("Entry: %08X - GP: %08X - TextAddr: %08X\n", info.entry_addr,
					info.gp_value, info.text_addr);
			Kprintf("TextSize: %08X - DataSize: %08X BssSize: %08X\n", info.text_size,
					info.data_size, info.bss_size);
		}
	}

	return ret;
}

static int modinfo_cmd(void)
{
	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;

	suid = strtok(NULL, " \t");
	if(suid != NULL)
	{
		char *endp;
		uid = strtoul(suid, &endp, 16);
		if(*endp == 0)
		{
			if(print_modinfo(uid, 1) < 0)
			{
				Kprintf("ERROR: Unknown module %08X\n", uid);
			}

			ret = CMD_OK;
		}
		else
		{
			Kprintf("ERROR: Invalid hex argument %s\n", suid);
		}
	}

	return ret;
}

static int modlist_cmd(void)
{
	SceUID ids[100];
	int ret;
	int count;
	int i;
	char *arg;
	int verbose = 0;

	arg = strtok(NULL, " ");
	if((arg != NULL) && (strcmp(arg, "v") == 0))
	{
		verbose = 1;
	}

	memset(ids, 0, 100 * sizeof(SceUID));
	ret = g_GetModuleIdList(ids, 100 * sizeof(SceUID), &count);
	if(ret >= 0)
	{
		Kprintf("<Module List>\n");
		for(i = 0; i < count; i++)
		{
			print_modinfo(ids[i], verbose);
		}
	}

	return CMD_OK;
}

static int modstop_cmd(void)
{
	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;

	suid = strtok(NULL, " \t");
	if(suid != NULL)
	{
		char *endp;
		uid = strtoul(suid, &endp, 16);
		if(*endp == 0)
		{
			SceUID uid_ret;
			int status;

			uid_ret = sceKernelStopModule(uid, 0, NULL, &status, NULL);
			Kprintf("Module Stop %08X Status %08X\n", uid_ret, status);

			ret = CMD_OK;
		}
		else
		{
			Kprintf("ERROR: Invalid hex argument %s\n", suid);
		}
	}

	return ret;
}

static int modunld_cmd(void)
{

	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;

	suid = strtok(NULL, " \t");
	if(suid != NULL)
	{
		char *endp;
		uid = strtoul(suid, &endp, 16);
		if(*endp == 0)
		{
			SceUID uid_ret;

			uid_ret = sceKernelUnloadModule(uid);
			Kprintf("Module Unload %08X\n", uid_ret);

			ret = CMD_OK;
		}
		else
		{
			Kprintf("ERROR: Invalid hex argument %s\n", suid);
		}
	}

	return ret;

}

static int modstart_cmd(void)
{
	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;

	suid = strtok(NULL, " \t");
	if(suid != NULL)
	{
		char *endp;
		uid = strtoul(suid, &endp, 16);
		if(*endp == 0)
		{
			SceUID uid_ret;
			int status;

			uid_ret = sceKernelStartModule(uid, 0, NULL, &status, NULL);
			Kprintf("Module Start %08X Status %08X\n", uid_ret, status);

			ret = CMD_OK;
		}
		else
		{
			Kprintf("ERROR: Invalid hex argument %s\n", suid);
		}
	}

	return ret;
}

static int modload_cmd(void)
{
	char *modname;
	int ret = CMD_ERROR;

	modname = strtok(NULL, " \t");
	if(modname != NULL)
	{
		SceUID modid;

		modid = sceKernelLoadModule(modname, 0, NULL);
		Kprintf("Module Load '%s' UID: %08X\n", modname, modid);
		ret = CMD_OK;
	}

	return ret;
}

static int modexec_cmd(void)
{
	char *modname;
	int ret = CMD_ERROR;

	modname = strtok(NULL, " \t");
	if(modname != NULL)
	{
		struct SceKernelLoadExecParam le;

		le.size = sizeof(le);
		le.args = strlen(modname) + 1;
		le.argp = modname;
		le.key = NULL;

		sceKernelLoadExec(modname, &le);

		ret = CMD_OK;
	}

	return ret;
}

int load_start_module(const char *name)
{
	SceUID modid;
	int status;

	modid = sceKernelLoadModule(name, 0, NULL);
	if(modid >= 0)
	{
		Kprintf("lsm: using name '%s'\n",name);
		modid = sceKernelStartModule(modid, strlen(name) + 1, (void *)name, &status, NULL);
	}

	return modid;
}

void psplinkResumeSh(void)
{
	sceKernelSetEventFlag(g_eventflag, EVENT_RESUMESH);
}

int load_start_module_debug(const char *name)
{
	SceUID modid;
	int status;

	modid = sceKernelLoadModule(name, 0, NULL);
	if(modid >= 0)
	{
		SceKernelModuleInfo info;
		int ret;

		ret = g_QueryModuleInfo(modid, &info);
		if(ret >= 0)
		{
			u32 result;

			g_debuggermode = 1;
			stop_usb();
			pspDebugGdbStubInit();

			set_swbp(info.entry_addr);
			sceKernelDcacheWBinvAll();
			sceKernelIcacheClearAll();
			Kprintf("lsmd: using name '%s'\n",name);
			modid = sceKernelStartModule(modid, strlen(name) + 1, (void *)name, &status, NULL);

			sceKernelWaitEventFlag(g_eventflag, EVENT_RESUMESH, 0x100, &result, NULL);
		}
	}

	return modid;
}

static int ldstart_cmd(void)
{
	char *modname;
	int ret = CMD_ERROR;

	modname = strtok(NULL, " \t");
	if(modname != NULL)
	{
		SceUID modid;

		modid = load_start_module(modname);
		if(modid >= 0)
		{
			Kprintf("Load/Start module UID: %08X\n", modid);
		}
		else
		{
			Kprintf("Failed to Load/Start module '%s' Error: %08X\n", modname, modid);
		}

		ret = CMD_OK;
	}

	return ret;
}

int build_args(char *args, const char *bootfile, const char *execfile)
{
	int loc = 0;

	strcpy(args, bootfile);
	loc += strlen(bootfile) + 1;
	if(execfile != NULL)
	{
		strcpy(&args[loc], execfile);
		loc += strlen(execfile) + 1;
	}

	return loc;
}

void psplinkReset(void)
{
	struct SceKernelLoadExecParam le;

	stop_usb();

	le.size = sizeof(le);
	le.args = strlen(g_bootfile) + 1;
	le.argp = (char *) g_bootfile;
	le.key = NULL;

	sceKernelLoadExec(g_bootfile, &le);
}

static int reset_cmd(void)
{
	psplinkReset();

	return CMD_OK;
}

static int exec_cmd(void)
{
	struct SceKernelLoadExecParam le;
	char args[512];
	char *file;
	int size;
	int ret = CMD_ERROR;

	do
	{
		file = strtok(NULL, " ");

		if(g_inexec)
		{
			if(file == NULL)
			{
				if(g_execfile[0] != 0)
				{
					file = (char *) g_execfile;
				}
				else
				{
					break;
				}
			}

			size = build_args(args, g_bootfile, file);

			stop_usb();

			le.size = sizeof(le);
			le.args = size;
			le.argp = (char *) args;
			le.key = NULL;

			sceKernelLoadExec(g_bootfile, &le);
		}
		else
		{
			SceUID modid;
			if(file == NULL)
			{
				break;
			}

			modid = load_start_module(file);
			if(modid >= 0)
			{
				Kprintf("Load/Start module UID: %08X\n", modid);
				strcpy(g_execfile, file);
				g_inexec = 1;
				ret = CMD_OK;
			}
			else
			{
				Kprintf("Failed to Load/Start module '%s' Error: %08X\n", file, modid);
			}
		}
	}
	while(0);

	return ret;
}

static int debug_cmd(void)
{
	char *file;
	int ret = CMD_ERROR;

	do
	{
		file = strtok(NULL, " ");

		if(g_inexec)
		{
			Kprintf("ERROR: Reset before going into debug mode\n");
			ret = CMD_OK;
			break;
		}
		else
		{
			SceUID modid;
			if(file == NULL)
			{
				break;
			}

			modid = load_start_module_debug(file);
			if(modid >= 0)
			{
				Kprintf("Load/Start module UID: %08X\n", modid);
				ret = CMD_OK;
			}
			else
			{
				Kprintf("Failed to Load/Start module '%s' Error: %08X\n", file, modid);
			}
		}
	}
	while(0);

	return ret;
}

static int exit_cmd(void)
{
	return CMD_EXITSHELL;
}

/* Help command */
static int help_cmd(void)
{
	int cmd_loop;
	char *cmd;

	cmd = strtok(NULL, " \t");

	if(cmd == NULL)
	{
		Kprintf("Command Help\n\n");
		for(cmd_loop = 0; commands[cmd_loop].name; cmd_loop++)
		{
			Kprintf("%s\t- %s\n", commands[cmd_loop].name, commands[cmd_loop].desc);
			if((cmd_loop % 24) == 20)
			{
				char ch;
				Kprintf("Press any key to continue, or q to quit\n");

				//while((ch = pspDebugSioGetchar()) == -1);
				while((ch = GetChar()) == -1);
				ch = upcase(ch);
				if(ch == 'Q')
				{
					break;
				}
			}
		}
	}
	else
	{
		struct sh_command* found_cmd;

		found_cmd = find_command(cmd);
		if(found_cmd != NULL)
		{
			Kprintf("%s\t - %s\n", found_cmd->name, found_cmd->desc);
			if(found_cmd->syn)
			{
				Kprintf("Synonym: %s\n", found_cmd->syn);
			}
			Kprintf("Usage: %s\n", found_cmd->help);
		}
	}

	return CMD_OK;
}

static void map_firmwarerev(void)
{
    if(sceKernelDevkitVersion() == 0x01050001)
	{
		g_QueryModuleInfo = sceKernelQueryModuleInfo;
		g_GetModuleIdList = sceKernelGetModuleIdList;
	}
	else
	{
		g_QueryModuleInfo = pspSdkQueryModuleInfoV1;
		g_GetModuleIdList = pspSdkGetModuleIdList;
	}
}


int init_usb(void)
{
	int retVal;

	do
	{
		load_start_module("flash0:/kd/semawm.prx");
		load_start_module("flash0:/kd/usbstor.prx");
		load_start_module("flash0:/kd/usbstormgr.prx");
		load_start_module("flash0:/kd/usbstorms.prx");
		load_start_module("flash0:/kd/usbstorboot.prx");

		retVal = sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
		if (retVal != 0) {
			Kprintf("Error starting USB Bus driver (0x%08X)\n", retVal);
			break;
		}
		retVal = sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0);
		if (retVal != 0) {
			Kprintf("Error starting USB Mass Storage driver (0x%08X)\n",
			   retVal);
			break;
		}
		retVal = sceUsbstorBootSetCapacity(0x800000);
		if (retVal != 0) {
			Kprintf
			("Error setting capacity with USB Mass Storage driver (0x%08X)\n",
			 retVal);
			break;
		}

		retVal = sceUsbActivate(0x1c8);
	}
	while(0);

	return retVal;
}

int stop_usb(void)
{
	int retVal;

	retVal = sceUsbDeactivate();
	if (retVal != 0) {
	    Kprintf("Error calling sceUsbDeactivate (0x%08X)\n", retVal);
    }

    retVal = sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);
    if (retVal != 0) {
		Kprintf("Error stopping USB Mass Storage driver (0x%08X)\n",
	       retVal);
	}

    retVal = sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
    if (retVal != 0) {
		Kprintf("Error stopping USB BUS driver (0x%08X)\n", retVal);
	}

	return 0;
}

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
		Kprintf("Couldn't find bootloader\n");
	}

	return 0;
}

#define MAX_ARGS 16

void parse_args(SceSize args, void *argp)
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
	g_bootfile = NULL;
	g_loaderthid = 0;
	memset(g_execfile, 0, sizeof(g_execfile));

	if(argc > 0)
	{
		g_bootfile = argv[0];
	}

	if(argc > 1)
	{
		char *endp;
		g_loaderthid = strtoul(argv[1], &endp, 16);
	}

	if(argc > 2)
	{
		strcpy(g_execfile, argv[2]);
	}
}

void load_psplink_user(const char *bootfile)
{
	char prx_path[256];
	char *path;

	path = strrchr(bootfile, '/');
	if(path != NULL)
	{
		memcpy(prx_path, bootfile, path - bootfile + 1);
		prx_path[path - bootfile + 1] = 0;
		strcat(prx_path, "psplink_user.prx");
		load_start_module(prx_path);
	}
}

int fdprintf(int fd, const char *fmt, ...);

/* Simple thread */
int main_thread(SceSize args, void *argp)
{
	DEBUG_START;
	DEBUG_PRINTF("Starting PSPLINK kernel module\n");
	map_firmwarerev();
	g_eventflag = sceKernelCreateEventFlag("SioShellEvent", 0, 0, 0);
	pspDebugSioInit();
	pspDebugSioSetBaud(115200);
	pspDebugInstallStdoutHandler(pspDebugSioPutText);
	pspDebugInstallStderrHandler(pspDebugSioPutText);
	pspDebugSioInstallKprintf();
	parse_args(args, argp);
	DEBUG_PRINTF("Bootfile %s threadid %08X execfile %s\n", g_bootfile, g_loaderthid,
			g_execfile[0] == 0 ? "NULL" : g_execfile);

	sceKernelWaitThreadEnd(g_loaderthid, NULL);
	unload_loader();

	load_psplink_user(g_bootfile);

	init_usb();

	if(g_execfile[0] != 0)
	{
		SceUID modid;

		modid = sceKernelLoadModule(g_execfile, 0, NULL);
		Kprintf("ExecFile UID: %08X\n", modid);
		if(modid >= 0)
		{
			int status;
			modid = sceKernelStartModule(modid, strlen(g_execfile) + 1, (void*) g_execfile, &status, NULL);
			Kprintf("ExecStart %08X\n", modid);
			g_inexec = 1;
		}
	}

	sceKernelRegisterIntrHandler(PSP_HPREMOTE_INT, 1, intr_handler, NULL, NULL);
	sceKernelEnableIntr(PSP_HPREMOTE_INT);

	sceKernelSetEventFlag(g_eventflag, EVENT_INIT);

	Kprintf(WELCOME_MESSAGE);

	shell();

	sceKernelExitGame();

	return 0;
}

int psplinkInitialised(void)
{
	u32 result;

	if(sceKernelWaitEventFlag(g_eventflag, EVENT_INIT, 0x1, &result, NULL) < 0)
	{
		return 0;
	}

	return 1;
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
