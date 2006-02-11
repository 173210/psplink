/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * apihook.c - User mode API hook code for psplink.
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
#include "apihook.h"

#define APIHOOK_MAXNAME 32
#define APIHOOK_MAXPARAM 6
#define APIHOOK_MAXIDS   16

/* Define the api hooks */
void _apiHook0(void);
void _apiHook1(void);
void _apiHook2(void);
void _apiHook3(void);
void _apiHook4(void);
void _apiHook5(void);
void _apiHook6(void);
void _apiHook7(void);
void _apiHook8(void);
void _apiHook9(void);
void _apiHook10(void);
void _apiHook11(void);
void _apiHook12(void);
void _apiHook13(void);
void _apiHook14(void);
void _apiHook15(void);

struct SyscallHeader
{
	void *unk;
	unsigned int basenum;
	unsigned int topnum;
	unsigned int size;
};

struct ApiHookGeneric
{
	/* Function name */
	char name[APIHOOK_MAXNAME];
	/* Parameter list */
	char param[APIHOOK_MAXPARAM];
	/* Pointer to the real function, if NULL the invalid */
	void *func;
	/* Pointer to the location in the syscall table */
	u32  *syscall;
	/* Pointer to the hook entry function */
	void *hookfunc;
	/* Indicates if we should sleep the thread on the syscall */
	int  sleep;
};

static struct ApiHookGeneric g_apihooks[APIHOOK_MAXIDS] = {
	{ "", "", NULL, NULL, _apiHook0, 0 },
	{ "", "", NULL, NULL, _apiHook1, 0 },
	{ "", "", NULL, NULL, _apiHook2, 0 },
	{ "", "", NULL, NULL, _apiHook3, 0 },
	{ "", "", NULL, NULL, _apiHook4, 0 },
	{ "", "", NULL, NULL, _apiHook5, 0 },
	{ "", "", NULL, NULL, _apiHook6, 0 },
	{ "", "", NULL, NULL, _apiHook7, 0 },
	{ "", "", NULL, NULL, _apiHook8, 0 },
	{ "", "", NULL, NULL, _apiHook9, 0 },
	{ "", "", NULL, NULL, _apiHook10, 0 },
	{ "", "", NULL, NULL, _apiHook11, 0 },
	{ "", "", NULL, NULL, _apiHook12, 0 },
	{ "", "", NULL, NULL, _apiHook13, 0 },
	{ "", "", NULL, NULL, _apiHook14, 0 },
	{ "", "", NULL, NULL, _apiHook15, 0 },
};

void *find_syscall_addr(u32 addr)
{
	struct SyscallHeader *head;
	u32 *syscalls;
	void **ptr;
	int size;
	int i;

	asm(
			"cfc0 %0, $12\n"
			: "=r"(ptr)
	   );

	if(!ptr)
	{
		return NULL;
	}

	head = (struct SyscallHeader *) *ptr;
	syscalls = (u32*) (*ptr + 0x10);
	size = (head->size - 0x10);

	for(i = 0; i < size; i++)
	{
		if(syscalls[i] == addr)
		{
			return &syscalls[i];
		}
	}

	return NULL;
}

void *_apiHookHandle(int id, u32 *args)
{
	int intc;
	void *func = NULL;

	intc = pspSdkDisableInterrupts();
	if((id >= 0) && (id < APIHOOK_MAXIDS))
	{
		func = g_apihooks[id].func;
	}
	pspSdkEnableInterrupts(intc);

	if(func)
	{
	}

	return func;
}

void apiHookGenericDelete(int id)
{
	int intc;

	if((id < 0) || (id >= APIHOOK_MAXIDS))
	{
		return;
	}

	intc = pspSdkDisableInterrupts();
	g_apihooks[id].func = NULL;
	/* Restore original function */
	pspSdkEnableInterrupts(intc);
}

void apiHookGenericPrint(void)
{
	int i;

	for(i = 0; i < APIHOOK_MAXIDS; i++)
	{
		if(g_apihooks[i].func)
		{
			printf("Hook %2d: Name %s, Param %.*s, Sleep %d\n", i,
					g_apihooks[i].name, APIHOOK_MAXPARAM, g_apihooks[i].param, 
					g_apihooks[i].sleep);
		}
	}
}

static int find_free_hook(void)
{
	int i;

	for(i = 0; i < APIHOOK_MAXIDS; i++)
	{
		if(!g_apihooks[i].func)
		{
			break;
		}
	}
	if(i < APIHOOK_MAXIDS)
	{
		return i;
	}

	return -1;
}

int apiHookGenericByName(SceUID uid, const char *library, const char *name, void *func, const char *format, int sleep)
{
	return 0;
}

int apiHookGenericByNid(SceUID uid, const char *library, u32 nid, void *func, const char *format, int sleep)
{
	int id;

	id = find_free_hook();
	if(id < 0)
	{
		printf("No free API hooks left\n");
		return 0;
	}

	return 0;
}

static void *apiHookAddr(u32 *addr, void *func)
{
	int intc;

	if(!addr)
	{
		return NULL;
	}

	intc = pspSdkDisableInterrupts();
	*addr = (u32) func;
	sceKernelDcacheWritebackInvalidateRange(addr, sizeof(addr));
	sceKernelIcacheInvalidateRange(addr, sizeof(addr));
	pspSdkEnableInterrupts(intc);

	return addr;
}

u32 apiHookByName(SceUID uid, const char *library, const char *name, void *func)
{
	u32 addr;

	addr = libsFindExportByName(uid, library, name);
	if(addr)
	{
		if(!apiHookAddr(find_syscall_addr(addr), func))
		{
			addr = 0;
		}
	}

	return addr;
}

u32 apiHookByNid(SceUID uid, const char *library, u32 nid, void *func)
{
	u32 addr;

	addr = libsFindExportByNid(uid, library, nid);
	if(addr)
	{
		if(!apiHookAddr(find_syscall_addr(addr), func))
		{
			addr = 0;
		}
	}

	return addr;
}
