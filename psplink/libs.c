/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * libs.c - Module library code for psplink.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsysmem_kernel.h>
#include <psputilsforkernel.h>
#include <pspmoduleexport.h>
#include <psploadcore.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "psplink.h"
#include "libs.h"

static struct SceLibraryEntryTable *_libsFindLibrary(SceUID uid, const char *library)
{
	struct SceLibraryEntryTable *entry;
	SceModule *pMod;
	void *entTab;
	int entLen;

	pMod = sceKernelFindModuleByUID(uid);
	if(pMod != NULL)
	{
		int i = 0;

		entTab = pMod->ent_top;
		entLen = pMod->ent_size;
		while(i < entLen)
		{
			entry = (struct SceLibraryEntryTable *) (entTab + i);

			if((entry->libname) && (strcmp(entry->libname, library) == 0))
			{
				return entry;
			}

			i += (entry->len * 4);
		}
	}

	return NULL;
}

u32 libsFindExportByNid(SceUID uid, const char *library, u32 nid)
{
	struct SceLibraryEntryTable *entry;
	u32 addr = 0;

	entry = _libsFindLibrary(uid, library);
	if(entry)
	{
		int count;
		int total;
		unsigned int *vars;

		total = entry->stubcount + entry->vstubcount;
		vars = entry->entrytable;

		if(entry->stubcount > 0)
		{
			for(count = 0; count < entry->stubcount; count++)
			{
				if(vars[count] == nid)
				{
					return vars[count+total];
				}
			}
		}
	}

	return addr;
}

u32 libsFindExportByName(SceUID uid, const char *library, const char *name)
{
	u8 digest[20];
	u32 nid;

	if(sceKernelUtilsSha1Digest((u8 *) name, strlen(name), digest) >= 0)
	{
		nid = digest[0] | (digest[1] << 8) | (digest[2] << 16) | (digest[3] << 24);
		printf("nid: %08X\n", nid);
		return libsFindExportByNid(uid, library, nid);
	}

	return 0;
}

int libsPrintEntries(SceUID uid)
{
	struct SceLibraryEntryTable *entry;
	SceModule *pMod;
	void *entTab;
	int entLen;

	pMod = sceKernelFindModuleByUID(uid);
	if(pMod != NULL)
	{
		int i = 0;

		entTab = pMod->ent_top;
		entLen = pMod->ent_size;
		printf("entTab %p - entLen %d\n", entTab, entLen);
		while(i < entLen)
		{
			int count;
			int total;
			unsigned int *vars;

			entry = (struct SceLibraryEntryTable *) (entTab + i);

			if(entry->libname)
			{
				printf("Export Library %s\n", entry->libname);
			}
			else
			{		
				printf("Export library syslib\n");
			}
			total = entry->stubcount + entry->vstubcount;
			vars = entry->entrytable;

			if(entry->stubcount > 0)
			{
				printf("Function Exports:\n");
				for(count = 0; count < entry->stubcount; count++)
				{
					printf("Entry %-3d: UID 0x%08X, Function 0x%08X\n", count+1, vars[count], vars[count+total]);
				}
			}

			if(entry->vstubcount > 0)
			{
				for(count = 0; count < entry->vstubcount; count++)
				{
					printf("Entry %-3d: UID 0x%08X, Variable 0x%08X\n", count+1, vars[count+entry->stubcount], 
							vars[count+entry->stubcount+total]);
				}
			}

			i += (entry->len * 4);
		}
	}
	else
	{
		return 0;
	}

	return 1;
}
