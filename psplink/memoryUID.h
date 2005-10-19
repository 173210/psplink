/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * memoryUID.h - Header file for UID dumper.
 *
 * Copyright (c) 2005 John_K
 *
 * $HeadURL$
 * $Id$
 */
#ifndef __MEMORYUID_H__
#define __MEMORYUID_H__

#define UIDLIST_START_1_0 0x8800d030
#define UIDLIST_START_1_5 0x8800fc98

struct _uidList {
    struct _uidList *parent;
    struct _uidList *nextChild;
    struct _uidList *unknown;   //(0x8)
    u32 UID;
    char *name;
    short unknown2;
    short attribute;
    struct _uidList *nextEntry;
};
typedef struct _uidList uidList;

u32 findUIDByName(const char *name);
void printUIDList(void);

#endif
