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
#include "memoryUID.h"
#include "psplink.h"
#include "psplinkcnf.h"
#include "parse_args.h"
#include "util.h"

PSP_MODULE_INFO("PSPLINK", 0x1000, 1, 1);

#define WELCOME_MESSAGE "PSPLINK Initialised\n"

#define SHELL_PROMPT	"psplink> "

#define BOOTLOADER_NAME "PSPLINKLOADER"

#define MAXPATHLEN      1024

#define MAX_ARGS 16

/* Maximum command line */
#define CLI_MAX			128
/* Maximum history */
#define CLI_HISTSIZE	8

enum UsbStates 
{
	USB_NOSTART = 0,
	USB_ON      = 1,
	USB_OFF     = 2
};

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
static char g_execfile[256];
/* Arguments for auto exec */
static int  g_execargc = 0;
static char *g_execargv[MAX_ARGS+1];
static char g_execargs[1024];
/* Inidicates whether a file has already been executed */
static int  g_inexec = 0;
/* Indicates the current directory */
static char g_currdir[1024];
/* The two instruction pre-amble from the original exitgame function */
static u32 g_exitgame[2];
/* Indicates whether the usb drivers have been loaded */
static enum UsbStates g_usbstate = USB_NOSTART;

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

int init_usb(void);
int stop_usb(void);
void save_execargs(int argc, char **argv);

struct psplink_config
{
	const char *name;
	int   isnum;
	void (*handler)(const char *szVal, unsigned int iVal);
};

static void config_usb(const char *szVal, unsigned int iVal);
static void config_baud(const char *szVal, unsigned int iVal);
static void config_modload(const char *szVal, unsigned int iVal);

struct psplink_config config_names[] = {
	{ "usb", 1, config_usb },
	{ "baud", 1, config_baud },
	{ "modload", 0, config_modload },
	{ NULL, 0, NULL }
};


static int exit_cmd(int argc, char **argv);
static int help_cmd(int argc, char **argv);
static int thlist_cmd(int argc, char **argv);
static int thinfo_cmd(int argc, char **argv);
static int evlist_cmd(int argc, char **argv);
static int evinfo_cmd(int argc, char **argv);
static int smlist_cmd(int argc, char **argv);
static int sminfo_cmd(int argc, char **argv);
static int uidlist_cmd(int argc, char **argv);
static int modlist_cmd(int argc, char **argv);
static int modinfo_cmd(int argc, char **argv);
static int modstop_cmd(int argc, char **argv);
static int modunld_cmd(int argc, char **argv);
static int modstart_cmd(int argc, char **argv);
static int modload_cmd(int argc, char **argv);
static int modexec_cmd(int argc, char **argv);
static int ldstart_cmd(int argc, char **argv);
static int exec_cmd(int argc, char **argv);
static int debug_cmd(int argc, char **argv);
static int reset_cmd(int argc, char **argv);
static int ls_cmd(int argc, char **argv);
static int chdir_cmd(int argc, char **argv);
static int pwd_cmd(int argc, char **argv);
static int usbon_cmd(int argc, char **argv);
static int usboff_cmd(int argc, char **argv);
static int cp_cmd(int argc, char **argv);
static int mkdir_cmd(int argc, char **argv);
static int rm_cmd(int argc, char **argv);
static int rmdir_cmd(int argc, char **argv);
static int rename_cmd(int argc, char **argv);

/* Return values for the commands */
#define CMD_EXITSHELL 	1
#define CMD_OK		  	0
#define CMD_ERROR		-1

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
struct sh_command commands[] = {
	{ "thlist", "tl", thlist_cmd, 0, "List the threads in the system", "tl [v]" },
	{ "thinfo", "ti", thinfo_cmd, 1, "Print info about a thread", "ti uid" },
	{ "evlist", "el", evlist_cmd, 0, "List the event flags in the system", "el [v]" },
	{ "evinfo", "ei", evinfo_cmd, 1, "Print info about an event flag", "ei uid" },
	{ "smlist", "sl", smlist_cmd, 0, "List the semaphores in the system", "sl [v]" },
	{ "sminfo", "si", sminfo_cmd, 1, "Print info about a semaphore", "si uid" },
    { "uidlist","ul", uidlist_cmd, 0, "List the system UIDS", "ul" },
	{ "modlist","ml", modlist_cmd, 0, "List the currently loaded modules", "ml [v]" },
	{ "modinfo","mi", modinfo_cmd, 1, "Print info about a module", "mi uid" },
	{ "modstop","ms", modstop_cmd, 1, "Stop a running module", "ms uid" },
	{ "modunld","mu", modunld_cmd, 1, "Unload a module (must be stopped)", "mu uid" },
	{ "modload","md", modload_cmd, 1, "Load a module", "md path" },
	{ "modstart","mt", modstart_cmd, 1, "Start a module", "mt uid [args]" },
	{ "modexec","me", modexec_cmd, 1, "LoadExec a module", "me path [args]" },
	{ "ldstart","ld", ldstart_cmd, 1, "Load and start a module", "ld path [args]" },
	{ "exec", "e", exec_cmd, 0, "Execute a new program (under psplink)", "exec [path] [args]" },
	{ "debug", "d", debug_cmd, 1, "Debug an executable (need to switch to gdb)", "debug path" },
	{ "ls",  "dir", ls_cmd,    0, "List the files in a directory", "ls [path]" },
	{ "chdir", "cd", chdir_cmd, 1, "Change the current directory", "cd path" },
	{ "cp",  "copy", cp_cmd, 2, "Copy a file", "cp source destination" },
	{ "mkdir", "md", mkdir_cmd, 1, "Make a Directory", "mkdir dir" },
	{ "rm", "del", rm_cmd, 1, "Removes a File", "rm file" },
	{ "rmdir", "rd", rmdir_cmd, 1, "Removes a Director", "rmdir dir" },
	{ "rename", "ren", rename_cmd, 2, "Renames a File", "rename src dst" },
	{ "pwd",   NULL, pwd_cmd, 0, "Print the current working directory", "pwd" },
	{ "usbon", "un", usbon_cmd, 0, "Enable USB mass storage device", "usbon" },
	{ "usboff", "uf", usboff_cmd, 0, "Disable USB mass storage device", "usboff" },
	{ "exit", "quit", exit_cmd, 0, "Exit the shell", "exit" },
	{ "reset", "r", reset_cmd, 0, "Reset", "r" },
	{ "help", "?", help_cmd, 0, "Help (Obviously)", "help [command]" },
	{ NULL, NULL, NULL, 0, NULL, NULL }
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
	int argc;
	char *argv[16];

    pspDebugSioPutchar(13);
    pspDebugSioPutchar(10);
	g_cli[g_cli_pos] = 0;
	memcpy(&g_lastcli[g_lastcli_pos][0], g_cli, CLI_MAX);
	g_lastcli_pos = (g_lastcli_pos + 1) % CLI_HISTSIZE;
	g_currcli_pos = g_lastcli_pos;

	if(parse_args(g_cli, &argc, argv, 16) == 0)
	{
		Kprintf("Error parsing cli\n");
		return CMD_ERROR;
	}

	if(argc > 0)
	{
		struct sh_command *found_cmd;

		cmd = argv[0];
		found_cmd = find_command(cmd);
		if(found_cmd)
		{
			if((found_cmd->min_args > (argc - 1)) || ((ret = found_cmd->func(argc-1, &argv[1])) == CMD_ERROR))
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

static int threadmaninfo_cmd(int argc, char **argv, const char *name, threadmanprint_func pinfo)
{
	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;
	char *endp;

	suid = argv[0];
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

static int thlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Thread, "Thread", print_threadinfo);
}

static int thinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Thread", print_threadinfo);
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

static int evlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_EventFlag, "EventFlag", print_eventinfo);
}

static int evinfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "EventFlag", print_eventinfo);
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

static int smlist_cmd(int argc, char **argv)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Semaphore, "Semaphore", print_semainfo);
}

static int sminfo_cmd(int argc, char **argv)
{
	return threadmaninfo_cmd(argc, argv, "Semaphore", print_semainfo);
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

static int modinfo_cmd(int argc, char **argv)
{
	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;
	char *endp;

	suid = argv[0];
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
		Kprintf("<Module List>\n");
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
	char *suid;
	int ret = CMD_ERROR;
	char *endp;

	suid = argv[0];
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

	return ret;
}

static int modunld_cmd(int argc, char **argv)
{

	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;
	char *endp;

	suid = argv[0];
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

	return ret;

}

static int modstart_cmd(int argc, char **argv)
{
	SceUID uid;
	char *suid;
	int ret = CMD_ERROR;
	char *endp;
	char args[1024];
	int  len;

	suid = argv[0];
	uid = strtoul(suid, &endp, 16);
	if(*endp == 0)
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
		Kprintf("Module Start %08X Status %08X\n", uid_ret, status);

		ret = CMD_OK;
	}
	else
	{
		Kprintf("ERROR: Invalid hex argument %s\n", suid);
	}

	return ret;
}

static int modload_cmd(int argc, char **argv)
{
	SceUID modid;
	char path[1024];

	if(handlepath(g_currdir, argv[0], path, TYPE_FILE, 1))
	{
		modid = sceKernelLoadModule(path, 0, NULL);
		Kprintf("Module Load '%s' UID: %08X\n", path, modid);
	}
	else
	{
		Kprintf("Error invalid file %s\n", path);
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
		if(handlepath(g_currdir, argv[0], path, TYPE_FILE, 1))
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

int load_start_module(const char *name, int argc, char **argv)
{
	SceUID modid;
	int status;
	char args[1024];
	int len;

	modid = sceKernelLoadModule(name, 0, NULL);
	if(modid >= 0)
	{
		len = build_args(args, name, argc, argv);
		modid = sceKernelStartModule(modid, len, (void *) args, &status, NULL);
		Kprintf("lsm: name '%s' ret %08X\n",name, modid);
	}
	else
	{
		Kprintf("lsm: Error loading module %s %08X\n", name, modid);
	}

	return modid;
}

void psplinkResumeSh(void)
{
	sceKernelSetEventFlag(g_eventflag, EVENT_RESUMESH);
}

void sceKernelIcacheInvalidateAll();

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
			sceKernelDcacheWritebackAll();
			sceKernelIcacheInvalidateAll();
			Kprintf("lsmd: using name '%s'\n",name);
			modid = sceKernelStartModule(modid, strlen(name) + 1, (void *)name, &status, NULL);

			sceKernelWaitEventFlag(g_eventflag, EVENT_RESUMESH, 0x100, &result, NULL);
		}
	}

	return modid;
}

static int ldstart_cmd(int argc, char **argv)
{
	char path[1024];
	int ret = CMD_ERROR;

	if(argc > 0)
	{
		SceUID modid;

		if(handlepath(g_currdir, argv[0], path, TYPE_FILE, 1))
		{
			modid = load_start_module(path, argc-1, &argv[1]);
			if(modid >= 0)
			{
				Kprintf("Load/Start module UID: %08X\n", modid);
			}
			else
			{
				Kprintf("Failed to Load/Start module '%s' Error: %08X\n", path, modid);
			}

			ret = CMD_OK;
		}
		else
		{
			Kprintf("Error invalid file %s\n", path);
		}
	}

	return ret;
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
		if((g_inexec) && (argc == 0))
		{
			exe = g_execfile;
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

		if(handlepath(g_currdir, exe, file, TYPE_FILE, 1) == 0)
		{
			Kprintf("Error, invalid file %s\n", file);
			break;
		}

		Kprintf("Exec '%s'\n", file);

		if(g_inexec)
		{
			if(argc == 0)
			{
				size = build_bootargs(args, g_bootfile, file, g_execargc, g_execargv);
			}
			else
			{
				size = build_bootargs(args, g_bootfile, file, argc-1, &argv[1]);
			}

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

			modid = load_start_module(file, argc-1, &argv[1]);
			if(modid >= 0)
			{
				Kprintf("Load/Start module UID: %08X\n", modid);
				strcpy(g_execfile, file);
				g_inexec = 1;
				save_execargs(argc-1, &argv[1]);
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

static int debug_cmd(int argc, char **argv)
{
	char *file;

	do
	{
		file = argv[0];

		if(g_inexec)
		{
			Kprintf("ERROR: Reset before going into debug mode\n");
		}
		else
		{
			SceUID modid;

			modid = load_start_module_debug(file);
			if(modid >= 0)
			{
				Kprintf("Load/Start module UID: %08X\n", modid);
			}
			else
			{
				Kprintf("Failed to Load/Start module '%s' Error: %08X\n", file, modid);
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
				Kprintf("d");
			}
			else
			{
				Kprintf("-");
			}

			for(ploop = 2; ploop >= 0; ploop--)
			{
				int bits;

				bits = (dir.d_stat.st_mode >> (ploop * 3)) & 0x7;
				if(bits & 4)
				{
					Kprintf("r");
				}
				else
				{
					Kprintf("-");
				}

				if(bits & 2)
				{
					Kprintf("w");
				}
				else
				{
					Kprintf("-");
				}

				if(bits & 1)
				{
					Kprintf("x");
				}
				else
				{
					Kprintf("-");
				}
			}

			Kprintf(" %8d ", (int) dir.d_stat.st_size);
			Kprintf("%02d-%02d-%04d %02d:%02d ", dir.d_stat.st_mtime.day, 
					dir.d_stat.st_mtime.month, dir.d_stat.st_mtime.year,
					dir.d_stat.st_mtime.hour, dir.d_stat.st_mtime.minute);
			Kprintf("%s\n", dir.d_name);
			memset(&dir, 0, sizeof(dir));
		}

		sceIoDclose(dfd);
	}
	else
	{
		Kprintf("Could not open directory '%s'\n", name);
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int ls_cmd(int argc, char **argv)
{
	char path[1024];

	if(argc == 0)
	{
		Kprintf("Listing directory %s\n", g_currdir);
		list_dir(g_currdir);
	}
	else
	{
		int loop;
		/* Strip whitespace and append a final slash */

		for(loop = 0; loop < argc; loop++)
		{
			if(handlepath(g_currdir, argv[loop], path, TYPE_DIR, 1))
			{
				Kprintf("Listing directory %s\n", path);
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

	if(handlepath(g_currdir, dir, path, TYPE_DIR, 1) == 0)
	{
		Kprintf("'%s' not a valid directory\n");
	}
	else
	{
		strcpy(g_currdir, path);
		ret = CMD_OK;
	}

	return ret;
}

static int pwd_cmd(int argc, char **argv)
{
	Kprintf("%s\n", g_currdir);

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

static int rename_cmd(int argc, char **argv)
{
	char asrc[MAXPATHLEN], adst[MAXPATHLEN];
	char *src, *dst;

	src = argv[0];
	dst = argv[1];

	if( !handlepath(g_currdir, src, asrc, TYPE_FILE, 1) )
		return CMD_ERROR;

	if( !handlepath(g_currdir, dst, adst, TYPE_FILE, 0) )
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

	if( !handlepath(g_currdir, file, afile, TYPE_FILE, 1) )
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

	if( !handlepath(g_currdir, file, afile, TYPE_FILE, 0) )
		return CMD_ERROR;

	if( sceIoMkdir(afile, 0777) == -1 )
		return CMD_ERROR;

	printf("mkdir %s\n", afile);

	return CMD_OK;
}

static int rmdir_cmd(int argc, char **argv)
{
	char *file, afile[MAXPATHLEN];

	file = argv[0];

	if( !handlepath(g_currdir, file, afile, TYPE_DIR, 1) )
		return CMD_ERROR;

	if( sceIoRmdir(afile) == -1 )
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

	if( !handlepath(g_currdir, source, fsrc, TYPE_FILE, 1) )
		return CMD_ERROR;
	
	if( !handlepath(g_currdir, destination, fdst, TYPE_ETHER, 0) )
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

static int exit_cmd(int argc, char **argv)
{
	return CMD_EXITSHELL;
}

/* Help command */
static int help_cmd(int argc, char **argv)
{
	int cmd_loop;

	if(argc < 1)
	{
		Kprintf("Command Help\n\n");
		for(cmd_loop = 0; commands[cmd_loop].name; cmd_loop++)
		{
			Kprintf("%10s - %s\n", commands[cmd_loop].name, commands[cmd_loop].desc);
			if((cmd_loop % 24) == 20)
			{
				char ch;
				Kprintf("Press any key to continue, or q to quit\n");

				while((ch = GetChar()) == -1);
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
		struct sh_command* found_cmd;

		found_cmd = find_command(argv[0]);
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
	/* Special case for version 1 firmware */
    if((sceKernelDevkitVersion() & 0xFFFF0000) == 0x01000000)
	{
		g_QueryModuleInfo = pspSdkQueryModuleInfoV1;
		g_GetModuleIdList = pspSdkGetModuleIdList;
	}
	else
	{
		g_QueryModuleInfo = sceKernelQueryModuleInfo;
		g_GetModuleIdList = sceKernelGetModuleIdList;
	}
}


int init_usb(void)
{
	int retVal;

	do
	{
		if(g_usbstate == USB_ON)
		{
			retVal = 0;
			break;
		}

		if(g_usbstate == USB_NOSTART)
		{
			load_start_module("flash0:/kd/semawm.prx", 0, NULL);
			load_start_module("flash0:/kd/usbstor.prx", 0, NULL);
			load_start_module("flash0:/kd/usbstormgr.prx", 0, NULL);
			load_start_module("flash0:/kd/usbstorms.prx", 0, NULL);
			load_start_module("flash0:/kd/usbstorboot.prx", 0, NULL);
		}

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

		if(retVal == 0)
		{
			g_usbstate = USB_ON;
		}
	}
	while(0);

	return retVal;
}

int stop_usb(void)
{
	int retVal;

	if(g_usbstate != USB_ON)
	{
		return 0;
	}

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

	g_usbstate = USB_OFF;

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

void save_execargs(int argc, char **argv)
{
	int i;
	int loc = 0;

	for(i = 0; i < (argc < MAX_ARGS ? argc : MAX_ARGS-1); i++)
	{
		strcpy(&g_execargs[loc], argv[i]);
		g_execargv[i] = &g_execargs[loc];
		loc += strlen(argv[i]) + 1;
	}

	argv[i] = NULL;
	g_execargc = argc;
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
		save_execargs(argc - 3, &argv[3]);
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
		load_start_module(prx_path, 0, NULL);
	}
}

void exit_reset(void)
{
	Kprintf("\nsceKernelExitGame caught!\n");
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

static void config_usb(const char *szVal, unsigned int iVal)
{
	if(iVal != 0)
	{
		init_usb();
	}
}

static void config_baud(const char *szVal, unsigned int iVal)
{
	int valid = 0;

	switch(iVal)
	{
		case 4800:
		case 9600:
		case 19200:
		case 38400:
		case 57600:
		case 115200: valid = 1;
					 break;
		default: break;
	};

	if(valid)
	{
		Kprintf("Setting baud to %d\n", iVal);
		pspDebugSioSetBaud(iVal);
	}
	else
	{
		/* Might never be seen :) */
		Kprintf("Invalid baud rate %d\n", iVal);
	}
}

static void config_modload(const char *szVal, unsigned int iVal)
{
	(void) load_start_module(szVal, 0, NULL);
}

void load_config(const char *bootfile)
{
	char cnf_path[256];
	char *path;
	struct ConfigFile cnf;

	path = strrchr(bootfile, '/');
	if(path != NULL)
	{
		memcpy(cnf_path, bootfile, path - bootfile + 1);
		cnf_path[path - bootfile + 1] = 0;
		strcat(cnf_path, "psplink.ini");
		Kprintf("Config Path %s\n", cnf_path);
		if(psplinkConfigOpen(cnf_path, &cnf))
		{
			const char *name;
			const char *val;

			while((val = psplinkConfigReadNext(&cnf, &name)))
			{
				int config;

				config = 0;
				while(config_names[config].name)
				{
					if(strcmp(config_names[config].name, name) == 0)
					{
						unsigned int iVal = 0;
						if(config_names[config].isnum)
						{
							char *endp;

							iVal = strtoul(val, &endp, 10);
							if(*endp != 0)
							{
								Kprintf("Error, line %d value should be a number\n", cnf.line); 
								break;
							}
						}

						config_names[config].handler(val, iVal);
					}
					config++;
				}

				/* Ignore anything we don't care about */
			}

			psplinkConfigClose(&cnf);
		}
	}
}

/* Simple thread */
int main_thread(SceSize args, void *argp)
{
	DEBUG_START;
	DEBUG_PRINTF("Starting PSPLINK kernel module\n");
	strcpy(g_currdir, "ms0:/");
	map_firmwarerev();
	g_eventflag = sceKernelCreateEventFlag("SioShellEvent", 0, 0, 0);
	pspDebugSioInit();
	pspDebugSioSetBaud(115200);
	pspDebugInstallStdoutHandler(pspDebugSioPutText);
	pspDebugInstallStderrHandler(pspDebugSioPutText);
	pspDebugSioInstallKprintf();
	sceUmdActivate(1, "disc0:");
	parse_sceargs(args, argp);
	DEBUG_PRINTF("Bootfile %s threadid %08X execfile %s\n", g_bootfile, g_loaderthid,
			g_execfile[0] == 0 ? "NULL" : g_execfile);
	patch_kernel();

	sceKernelWaitThreadEnd(g_loaderthid, NULL);
	unload_loader();

	load_config(g_bootfile);
	load_psplink_user(g_bootfile);

	if(g_execfile[0] != 0)
	{
		if(load_start_module(g_execfile, g_execargc, g_execargv) >= 0)
		{
			g_inexec = 1;
		}
	}

	sceKernelRegisterIntrHandler(PSP_HPREMOTE_INT, 1, intr_handler, NULL, NULL);
	sceKernelEnableIntr(PSP_HPREMOTE_INT);

	sceKernelSetEventFlag(g_eventflag, EVENT_INIT);

	Kprintf(WELCOME_MESSAGE);

	shell();

	unpatch_kernel();
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
