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
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "psplink.h"
#include "libs.h"

struct PspModuleImport
{
	const char *name;
	unsigned short version;
	unsigned short attribute;
	unsigned char entLen;
	unsigned char varCount;
	unsigned short funcCount;
	u32 *fnids;
	u32 *funcs;
	u32 *vnids;
	u32 *vars;
} __attribute__((packed));

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
			else if(!entry->libname && !library)
			{
				return entry;
			}

			i += (entry->len * 4);
		}
	}

	return NULL;
}

void* libsFindExportAddrByNid(SceUID uid, const char *library, u32 nid)
{
	struct SceLibraryEntryTable *entry;
	u32 *addr = NULL;

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
					return &vars[count+total];
				}
			}
		}
	}

	return addr;
}

u32 libsNameToNid(const char *name)
{
	u8 digest[20];
	u32 nid;

	if(sceKernelUtilsSha1Digest((u8 *) name, strlen(name), digest) >= 0)
	{
		nid = digest[0] | (digest[1] << 8) | (digest[2] << 16) | (digest[3] << 24);
		return nid;
	}

	return 0;
}

void* libsFindExportAddrByName(SceUID uid, const char *library, const char *name)
{
	u32 nid;

	nid = libsNameToNid(name);

	return libsFindExportAddrByNid(uid, library, nid);
}

u32 libsFindExportByName(SceUID uid, const char *library, const char *name)
{
	u32 *addr;

	addr = libsFindExportAddrByName(uid, library, name);
	if(!addr)
	{
		return 0;
	}

	return *addr;
}

u32 libsFindExportByNid(SceUID uid, const char *library, u32 nid)
{
	u32 *addr;

	addr = libsFindExportAddrByNid(uid, library, nid);
	if(!addr)
	{
		return 0;
	}

	return *addr;
}

int libsPatchFunction(SceUID uid, const char *library, u32 nid, u16 retval)
{
	u32* addr;
	int intc;
	int ret = 0;

	intc = pspSdkDisableInterrupts();
	addr = (u32 *) libsFindExportByNid(uid, library, nid);
	if(addr)
	{
		addr[0] = 0x03E00008;
		addr[1] = 0x24020000 | (u32) retval;
		sceKernelDcacheWritebackInvalidateRange(addr, 8);
		sceKernelIcacheInvalidateRange(addr, 8);
		ret = 1;
	}
	pspSdkEnableInterrupts(intc);

	return ret;
}

int libsPrintImports(SceUID uid)
{
	SceModule *pMod;
	void *stubTab;
	int stubLen;

	pMod = sceKernelFindModuleByUID(uid);
	if(pMod != NULL)
	{
		int i = 0;

		stubTab = pMod->stub_top;
		stubLen = pMod->stub_size;
		printf("stubTab %p - stubLen %d\n", stubTab, stubLen);
		while(i < stubLen)
		{
			int count;
			struct PspModuleImport *pImp = (struct PspModuleImport *) (stubTab + i);

			if(pImp->name)
			{
				printf("Import Library %s, attr 0x%04X\n", pImp->name, pImp->attribute);
			}
			else
			{
				printf("Import Library %s, attr 0x%04X\n", "Unknown", pImp->attribute);
			}

			if(pImp->funcCount > 0)
			{
				printf("Function Imports:\n");
				for(count = 0; count < pImp->funcCount; count++)
				{
					printf("Entry %-3d: UID 0x%08X, Function 0x%08X\n", count+1, pImp->fnids[count], 
							(u32) &pImp->funcs[count*2]);
				}
			}

			if(pImp->funcCount > 0)
			{
				printf("Variable Imports:\n");
				for(count = 0; count < pImp->varCount; count++)
				{
					printf("Entry %-3d: UID 0x%08X, Variable 0x%08X\n", count+1, pImp->vnids[count], 
							(u32) &pImp->vars[count*2]);
				}
			}
			printf("\n");

			i += (pImp->entLen * 4);
		}
	}
	else
	{
		return 0;
	}

	return 1;
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
				printf("Export Library %s, attr 0x%04X\n", entry->libname, entry->attribute);
			}
			else
			{		
				printf("Export library %s, attr 0x%04X\n", "syslib", entry->attribute);
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
				printf("Variable Exports:\n");
				for(count = 0; count < entry->vstubcount; count++)
				{
					printf("Entry %-3d: UID 0x%08X, Variable 0x%08X\n", count+1, vars[count+entry->stubcount], 
							vars[count+entry->stubcount+total]);
				}
			}
			printf("\n");

			i += (entry->len * 4);
		}
	}
	else
	{
		return 0;
	}

	return 1;
}
