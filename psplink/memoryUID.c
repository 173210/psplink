/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * memoryUID.c - Code to dump the UID table.
 *
 * Copyright (c) 2005 John_K
 *
 * $HeadURL$
 * $Id$
 */
#include <pspkernel.h>
#include <pspsysmem.h>
#include <stdio.h>
#include <string.h>
#include "memoryUID.h"

u32 findUIDByName(const char *name)
{
    uidList *entry;
    uidList *end;
    if(sceKernelDevkitVersion() == 0x01050001)
    	entry = (uidList *) UIDLIST_START_1_5;
    else
    	entry = (uidList *) UIDLIST_START_1_0;
    entry = entry->parent;
    end = entry;
    entry = entry->nextEntry;

    do {
        if (strcmp(entry->name, name) == 0)
            return entry->UID;
        if (entry->nextChild != entry) {
            do {
                entry = entry->nextChild;
                if (strcmp(entry->name, name) == 0)
                    return entry->UID;
            } while (entry->nextChild != entry->unknown);
            entry = entry->nextChild;
        }
        entry = entry->nextEntry;
    } while (entry->nextEntry != end);  //Stop at 'Basic' entry like Sysmem does, just not in the same way ;)
    return 0;
}

uidList* findObjectByUID(SceUID uid)
{
    uidList *entry;
    uidList *end;
    if(sceKernelDevkitVersion() == 0x01050001)
    	entry = (uidList *) UIDLIST_START_1_5;
    else
    	entry = (uidList *) UIDLIST_START_1_0;
    entry = entry->parent;
    end = entry;
    entry = entry->nextEntry;

    do {
		if(entry->UID == uid)
            return entry;
        if (entry->nextChild != entry) {
            do {
                entry = entry->nextChild;
				if(entry->UID == uid)
					return entry;
            } while (entry->nextChild != entry->unknown);
            entry = entry->nextChild;
        }
        entry = entry->nextEntry;
    } while (entry->nextEntry != end);  //Stop at 'Basic' entry like Sysmem does, just not in the same way ;)
    return 0;
}

void printUIDList(const char *name)
{
    uidList *entry;
    uidList *end;
	int cmp = 0;
    if(sceKernelDevkitVersion() == 0x01050001)
    	entry = (uidList *) UIDLIST_START_1_5;
    else
    	entry = (uidList *) UIDLIST_START_1_0;
    entry = entry->parent;
    end = entry;
    entry = entry->nextEntry;
    printf("************ MY UID LIST START ************\n");
    do {
		if(name)
		{
			cmp = strcmp(entry->name, name);
		}

		if(cmp == 0)
		{
			printf("\n[%s]    UID 0x%08X (attr 0x%X entry 0x%p)\n", entry->name, entry->UID, entry->attribute, entry);
		}

		if (entry->nextChild == entry) {
			if(cmp == 0)
			{
				printf("    <No UID objects>\n");
			}

		} else {
			do {
				entry = entry->nextChild;
				if(cmp == 0)
				{
					printf("  --  (Name): %31s, (UID): 0x%08X, (entry): 0x%p (attr): 0x%X \n", entry->name, entry->UID, entry, entry->attribute);
				}
			} while (entry->nextChild != entry->unknown);
			entry = entry->nextChild;
		}

		entry = entry->nextEntry;
    } while (entry->nextEntry != end);  //Stop at 'Basic' entry like Sysmem does but not in the same way ;)
    printf("************ MY UID LIST END ************\n");
}

