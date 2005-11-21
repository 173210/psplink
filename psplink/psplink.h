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

#endif
