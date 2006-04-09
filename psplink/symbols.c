/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * symbols.c - Symbol managment code for psplink
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
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "psplink.h"
#include "symbols.h"

/* TODO: Perhaps needs some sort of lock (disable interrupts?) */

#define SYMFILE_MAGIC "SYMS"
#define MODNAME_SIZE  28

struct SymfileHeader
{
	char magic[4];
	char modname[MODNAME_SIZE];
	u32  symcount;
	u32  strstart;
	u32  strsize;
} __attribute__((packed));

struct SymbolFile
{
	struct SymbolFile *pNext, *pPrev;
	void *data;
	unsigned int size;
};

#define HEAP_SIZE (64*1024)
#define HEAP_PARTITION 1

static SceUID g_block_id = -1;
static void *g_baseaddr = NULL;
static void *g_curraddr = NULL;
static struct SymbolFile *g_pHead = NULL;

/* TODO: If we have run out of memory try and coallecse any free space */
static struct SymbolFile *alloc_symbolfile(unsigned int size)
{
	struct SymbolFile *pRet = NULL;
	unsigned int total_size;

	if(g_block_id < 0)
	{
		g_block_id = sceKernelAllocPartitionMemory(HEAP_PARTITION, "symbols", PSP_SMEM_Low, HEAP_SIZE, NULL);
		if(g_block_id < 0)
		{
			printf("Error could not allocate memory buffer %08X\n", g_block_id);
			return NULL;
		}
		g_baseaddr = g_curraddr = sceKernelGetBlockHeadAddr(g_block_id);
	}

	total_size = ((size + 3) & ~3) + sizeof(struct SymbolFile);

	if((g_curraddr + size) < (g_baseaddr + HEAP_SIZE))
	{
		pRet = g_curraddr;
		memset(pRet, 0, total_size);
		pRet->size = size;
		pRet->data = g_curraddr + sizeof(struct SymbolFile);
		if(g_pHead == NULL)
		{
			g_pHead = pRet;
		}
		else
		{
			struct SymbolFile *pFile;

			pFile = g_pHead;
			while(pFile->pNext)
			{
				pFile = pFile->pNext;
			}
			pFile->pNext = pRet;
			pRet->pPrev = pFile;
		}

		g_curraddr += total_size;
	}
	else
	{
		printf("Error not enough memory to allocate %d bytes\n", size);
	}

	return pRet;
}

static void free_symbolfile(struct SymbolFile *pSym)
{
	if(pSym)
	{
		if(pSym->pPrev == NULL)
		{
			g_pHead = pSym->pNext;
		}
		else
		{
			pSym->pPrev->pNext = pSym->pNext;
		}

		if(pSym->pNext)
		{
			pSym->pNext->pPrev = pSym->pPrev;
		}
	}
}

static struct SymbolFile *find_symbolfile(const char *modname)
{
	struct SymbolFile *pNext;
	char name[MODNAME_SIZE+1];

	if(strlen(modname) > MODNAME_SIZE)
	{
		return NULL;
	}

	pNext = g_pHead;

	while(pNext)
	{
		struct SymfileHeader *pHead;

		pHead = (struct SymfileHeader *) pNext->data;
		memcpy(name, pHead->modname, MODNAME_SIZE);
		name[MODNAME_SIZE] = 0;

		if(strcmp(name, modname) == 0)
		{
			break;
		}

		pNext = pNext->pNext;
	}

	return pNext;
}

static void free_mem(void)
{
	if(g_block_id >= 0)
	{
		sceKernelFreePartitionMemory(g_block_id);
		g_block_id = -1;
	}
}

const struct SymfileEntry* symbolFindByAddress(unsigned int addr, unsigned int *baseaddr)
{
	SceKernelModuleInfo info;
	SceUID uid;
	struct SymfileEntry *pRet = NULL;

	uid = refer_module_by_addr(addr, &info);
	if(uid >= 0)
	{
		struct SymbolFile *pFile;

		pFile = find_symbolfile(info.name);
		if(pFile)
		{
			struct SymfileHeader *pHead;
			struct SymfileEntry  *pEntry;
			unsigned int ba;
			int i;

			pHead = (struct SymfileHeader *) pFile->data;
			pEntry = (struct SymfileEntry *) (pFile->data + sizeof(struct SymfileHeader));
			ba = info.text_addr;
			/* TODO: Really should do a quick search */
			for(i = 0; i < pHead->symcount; i++)
			{
				unsigned int bottom, top;

				if(pEntry[i].addr >= 0x08000000)
				{
					bottom = pEntry[i].addr;
				}
				else
				{
					bottom = pEntry[i].addr + ba;
				}

				if((pEntry[i].size == 0) && (i < (pHead->symcount-1)))
				{
					top = pEntry[i+1].addr;
				}
				else
				{
					top = pEntry[i].size + bottom;
				}

				if((addr >= bottom) && (addr < top))
				{
					pRet = &pEntry[i];
					/* Return base address in addr */
					if(baseaddr)
					{
						*baseaddr = ba;
					}
					break;
				}
			}
		}
		else
		{
			//printf("Error could not find symbol file for module %s\n", info.name);
		}
	}
	else
	{
		//printf("Error could not find module at addr %08X\n", addr);
	}

	return pRet;
}

const char *symbolFindNameByAddress(unsigned int addr)
{
	const struct SymfileEntry *sym;

	sym = symbolFindByAddress(addr, NULL);
	if(sym)
	{
		return sym->name;
	}

	return "Unknown";
}

int symbolFindNameByAddressEx(unsigned int addr, char *output, int size)
{
	const struct SymfileEntry *pSym;
	unsigned int baseaddr;
	char symtemp[256];

	if((output == NULL) || (size <= 0))
	{
		return 0;
	}

	pSym = symbolFindByAddress(addr, &baseaddr);
	if(pSym)
	{
		if((baseaddr + pSym->addr) < addr)
		{
			sprintf(symtemp, "%s+0x%x", pSym->name,
					addr - (baseaddr + pSym->addr));
		}
		else
		{
			sprintf(symtemp, "%s", pSym->name);
		}
	}
	else
	{
		return 0;
	}

	strncpy(output, symtemp, size);
	output[size-1] = 0;

	return 1;
}

unsigned int symbolFindByName(const char *name)
{
	const char *modname = NULL;
	const char *symname = NULL;
	char symbuf[128];
	char *pcolon;
	struct SymbolFile *pFile;
	unsigned int addr = 0;
	SceUID uid;
	SceKernelModuleInfo info;

	strncpy(symbuf, name, 127);
	symbuf[127] = 0;
	pcolon = strchr(symbuf, ':');
	if(pcolon)
	{
		*pcolon = 0;
		pcolon++;
		modname = symbuf;
		symname = pcolon;
	}
	else
	{
		printf("Error symbol address must be of the form module:name\n");
		return 0;
	}

	uid = refer_module_by_name(modname, &info);
	if(uid < 0)
	{
		printf("Error cannot find loaded module %s\n", modname);
		return 0;
	}

	pFile = find_symbolfile(modname);
	if(pFile)
	{
		struct SymfileHeader *pHead;
		struct SymfileEntry *pEntry;
		int i;

		pHead = (struct SymfileHeader *) pFile->data;
		pEntry = (struct SymfileEntry *) (pFile->data + sizeof(struct SymfileHeader));

		for(i = 0; i < pHead->symcount; i++)
		{
			if(strcmp(symname, pEntry[i].name) == 0)
			{
				if(pEntry[i].addr >= 0x08000000)
				{
					addr = pEntry[i].addr;
				}
				else
				{
					addr = info.text_addr + pEntry[i].addr;
				}
				break;
			}
		}
	}
	else
	{
		printf("Error could not file module %s's symbols\n", modname);
	}

	return addr;
}

void symbolPrintLoadList(void)
{
	if(g_pHead)
	{
		struct SymbolFile *pNext;

		pNext = g_pHead;
		while(pNext)
		{
			struct SymfileHeader *pHead;

			pHead = (struct SymfileHeader *) pNext->data;
			printf("Module %.28s - Size %d - Symcount %d\n", pHead->modname, pNext->size, pHead->symcount);
			pNext = pNext->pNext;
		}
	}
	else
	{
		printf("No symbols loaded\n");
	}
}

void symbolPrintSymbols(const char *modname)
{
	struct SymbolFile *pFile;

	pFile = find_symbolfile(modname);
	if(pFile)
	{
		struct SymfileHeader *pHead;
		struct SymfileEntry  *pEntry;
		int i;

		pHead = (struct SymfileHeader *) pFile->data;
		pEntry = (struct SymfileEntry *) (pFile->data + sizeof(struct SymfileHeader));
		for(i = 0; i < pHead->symcount; i++)
		{
			printf("Symbol %d - Address 0x%08X - Size %-10d - Name %s\n", i, 
					pEntry[i].addr, pEntry[i].size, pEntry[i].name);
		}
	}
	else
	{
		printf("Could not file module %s's symbols\n", modname);
	}
}

int symbolLoadSymbols(const char *file)
{
	SceIoStat st;
	struct SymbolFile *pFile = NULL;
	struct SymfileHeader *pHead = NULL;
	struct SymfileEntry  *pEntry = NULL;
	const char *pString = NULL;
	int fd = -1;
	int ret;
	int i;

	memset(&st, 0, sizeof(st));
	if(sceIoGetstat(file, &st) < 0)
	{
		printf("Error file %s does not exist\n", file);
		goto error;
	}

	pFile = alloc_symbolfile(st.st_size);
	if(pFile == NULL)
	{
		printf("Error could not allocate memory for symbol file\n");
		goto error;
	}

	fd = sceIoOpen(file, PSP_O_RDONLY, 0777);
	if(fd < 0)
	{
		printf("Error could not open file %s - %08X\n", file, fd);
		goto error;
	}

	ret = sceIoRead(fd, pFile->data, st.st_size);
	if(ret != st.st_size)
	{
		printf("Error reading data from file\n");
		goto error;
	}

	sceIoClose(fd);
	fd = -1;

	pHead = (struct SymfileHeader *) pFile->data;
	if(memcmp(pHead->magic, SYMFILE_MAGIC, 4) != 0)
	{
		printf("Error invalid magic\n");
		goto error;
	}

	if(((pHead->symcount * sizeof(struct SymfileEntry)) + pHead->strsize + sizeof(struct SymfileHeader)) != pFile->size)
	{
		printf("Error invalid size for symbol file\n");
		goto error;
	}

	/* Fixup string lists */
	pEntry = (struct SymfileEntry *) (pFile->data + sizeof(struct SymfileHeader));
	pString = (const char *) (pFile->data + pHead->strstart);

	for(i = 0; i < pHead->symcount; i++)
	{
		pEntry[i].name = &pString[(u32) pEntry[i].name];
	}

	return 1;

error:
	if(pFile)
	{
		free_symbolfile(pFile);
	}

	if(fd >= 0)
	{
		sceIoClose(fd);
	}

	return 0;
}

/* TODO: Actually allow the deletion of a single symbol table */
int symbolDeleteSymbols(const char *modname)
{
	return 0;
}

void symbolDeleteAll(void)
{
	free_mem();
	g_pHead = NULL;
}
