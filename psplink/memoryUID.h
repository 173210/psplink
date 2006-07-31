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
    struct _uidList *realParent;   //(0x8)
    u32 UID;					//(0xC)
    char *name;					//(0x10)
	unsigned char unk;
	unsigned char size;			// Size in words
    short attribute;
    struct _uidList *nextEntry;
} __attribute__((packed));
typedef struct _uidList uidList;

uidList* findUIDObject(SceUID uid, const char *name, const char *parent);
SceUID findUIDByName(const char *name);
void printUIDList(const char *name);
void printUIDEntry(uidList *entry);

#define findObjectByUID(uid) findUIDObject(uid, NULL, NULL)
#define findObjectByName(name) findUIDObject(0, name, NULL)
#define findObjectByUIDWithParent(uid, parent) findUIDObject(uid, NULL, parent)
#define findObjectByNameWithParent(name, parent) findUIDObject(0, name, parent)

#endif
