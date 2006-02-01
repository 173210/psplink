/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psplink.h - PSPLINK global header file.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#ifndef __PSPLINK_H
#define __PSPLINK_H

/* Event flags */
#define EVENT_SIO       0x01
#define EVENT_INIT      0x10
#define EVENT_RESUMESH  0x100
#define EVENT_RESET     0x200

#define MAXPATHLEN      1024
#define MAX_ARGS 16

#define DEFAULT_BAUDRATE 115200

#ifdef DEBUG
#define DEBUG_START { int fd; fd = sceIoOpen("ms0:/debug.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666); sceIoClose(fd); }
#define DEBUG_PRINTF(fmt, ...) \
{ \
	int fd; \
	fd = sceIoOpen("ms0:/debug.txt", PSP_O_WRONLY | PSP_O_APPEND, 0666); \
	fdprintf(fd, fmt, ## __VA_ARGS__); \
	sceIoClose(fd); \
}
#else
#define DEBUG_START
#define DEBUG_PRINTF(fmt, ...)
#endif

int fdprintf(int fd, const char *fmt, ...);
char *PSPdis(unsigned long _PC);

void psplinkReset(void);
u32  psplinkSetK1(u32 k1);
void psplinkGetCop0(u32 *regs);
int psplinkParseComamnd(char *command);

struct GlobalContext
{
	/* The filename of the bootstrap */
	const char *bootfile;
	/* The boot path */
	char bootpath[MAXPATHLEN];
	/* Indicates the current directory */
	char currdir[MAXPATHLEN];
	/* Arguments for auto exec */
	int  execargc;
	char *execargv[MAX_ARGS+1];
	char execargs[1024];
	/* Inidicates whether a file has already been executed */
	int  inexec;
	/* The program to execute */
	char execfile[MAXPATHLEN];
	int resetonexit;
	int pcterm;
	SceUID netshelluid;
};

#endif
