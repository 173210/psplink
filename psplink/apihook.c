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
#include "decodeaddr.h"

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

#define PARAM_TYPE_INT 'i'
#define PARAM_TYPE_HEX 'x'
#define PARAM_TYPE_OCT 'o'
#define PARAM_TYPE_STR 's'

#define RET_TYPE_VOID  'v'
#define RET_TYPE_HEX32 'x'
#define RET_TYPE_HEX64 'X'
#define RET_TYPE_INT32 'i'

struct ApiHookGeneric
{
	/* Function name */
	char name[APIHOOK_MAXNAME];
	/* Parameter list */
	char param[APIHOOK_MAXPARAM];
	/* Return code */
	char ret;
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
	{ "", "", 'v', NULL, NULL, _apiHook0, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook1, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook2, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook3, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook4, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook5, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook6, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook7, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook8, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook9, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook10, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook11, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook12, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook13, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook14, 0 },
	{ "", "", 'v', NULL, NULL, _apiHook15, 0 },
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
	size = (head->size - 0x10) / sizeof(u32);

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
	int k1;

	intc = pspSdkDisableInterrupts();
	if((id >= 0) && (id < APIHOOK_MAXIDS))
	{
		func = g_apihooks[id].func;
	}
	pspSdkEnableInterrupts(intc);

	k1 = psplinkSetK1(0);

	if(func)
	{
		int i;
		char str[128];
		int strleft;

		printf("Function %s called from thread 0x%08X\n", g_apihooks[id].name, sceKernelGetThreadId());
		for(i = 0; i < APIHOOK_MAXPARAM; i++)
		{
			if(g_apihooks[id].param[i])
			{
				printf("Arg %d: ", i);
				switch(g_apihooks[id].param[i])
				{
					case PARAM_TYPE_INT: printf("%d\n", args[i]);
							  break;
					case PARAM_TYPE_HEX: printf("0x%08X\n", args[i]);
							  break;
					case PARAM_TYPE_OCT: printf("0%o\n", args[i]);
							  break;
					case PARAM_TYPE_STR: strleft = memValidate(args[i], MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
							  if(strleft == 0)
							  {
								  printf("Invalid pointer 0x%08X\n", args[i]);
								  break;
							  }

							  if(strleft > (sizeof(str)-1))
							  {
								  strleft = sizeof(str) - 1;
							  }

							  strncpy(str, (const char *) args[i], strleft);
							  str[strleft] = 0;

							  printf("\"%s\"\n", str);
							  break;
					default:  printf("Unknown parameter type '%c'\n", g_apihooks[id].param[i]);
							  break;
				};
			}
			else
			{
				break;
			}
		}

		if(g_apihooks[id].sleep)
		{
			printf("Sleeping thread 0x%08X\n", sceKernelGetThreadId());
			sceKernelSleepThread();
		}
	}

	psplinkSetK1(k1);

	return func;
}

void _apiHookReturn(int id, u32* ret)
{
	int intc;
	void *func = NULL;
	int k1;

	intc = pspSdkDisableInterrupts();
	if((id >= 0) && (id < APIHOOK_MAXIDS))
	{
		func = g_apihooks[id].func;
	}
	pspSdkEnableInterrupts(intc);

	k1 = psplinkSetK1(0);

	if(func)
	{
		printf("Function %s returned ", g_apihooks[id].name);
		switch(g_apihooks[id].ret)
		{
			case RET_TYPE_INT32: printf("%d\n", ret[0]);
					  break;
			case RET_TYPE_HEX32: printf("0x%08X\n", ret[0]);
								 break;
			case RET_TYPE_HEX64: printf("0x%08X%08X\n", ret[1], ret[0]);
					  break;
			default: printf("void\n");
					break;
		}

		if(g_apihooks[id].sleep)
		{
			printf("Sleeping thread 0x%08X\n", sceKernelGetThreadId());
			sceKernelSleepThread();
		}
	}

	psplinkSetK1(k1);
}

void apiHookGenericDelete(int id)
{
	int intc;

	if((id < 0) || (id >= APIHOOK_MAXIDS))
	{
		return;
	}

	intc = pspSdkDisableInterrupts();
	/* Restore original function */
	*g_apihooks[id].syscall = (u32) g_apihooks[id].func;
	g_apihooks[id].func = NULL;
	sceKernelDcacheWritebackInvalidateRange(g_apihooks[id].syscall, sizeof(void *));
	sceKernelIcacheInvalidateRange(g_apihooks[id].syscall, sizeof(void *));
	pspSdkEnableInterrupts(intc);
}

void apiHookGenericPrint(void)
{
	int i;

	for(i = 0; i < APIHOOK_MAXIDS; i++)
	{
		if(g_apihooks[i].func)
		{
			printf("Hook %2d: Name %s, Param %.*s, Sleep %d, Syscall 0x%p\n", i,
					g_apihooks[i].name, APIHOOK_MAXPARAM, g_apihooks[i].param, 
					g_apihooks[i].sleep, g_apihooks[i].syscall);
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

int apiHookGenericByName(SceUID uid, const char *library, const char *name, char ret, const char *format, int sleep)
{
	int id;
	u32 addr;
	u32 *syscall;

	id = find_free_hook();
	if(id < 0)
	{
		printf("No free API hooks left\n");
		return 0;
	}

	addr = libsFindExportByName(uid, library, name);
	if(addr)
	{
		syscall = find_syscall_addr(addr);
		if(syscall)
		{
			g_apihooks[id].syscall = syscall;
			g_apihooks[id].func = (void *) addr;
			g_apihooks[id].ret = ret;
			g_apihooks[id].sleep = sleep;
			strncpy(g_apihooks[id].param, format, APIHOOK_MAXPARAM);
			strncpy(g_apihooks[id].name, name, APIHOOK_MAXNAME);
			g_apihooks[id].name[APIHOOK_MAXNAME-1] = 0;
			apiHookAddr(syscall, g_apihooks[id].hookfunc);

			return 1;
		}
		else
		{
			printf("Couldn't find syscall address\n");
		}
	}
	else
	{
		printf("Couldn't find export address\n");
	}

	return 0;
}

int apiHookGenericByNid(SceUID uid, const char *library, u32 nid, char ret, const char *format, int sleep)
{
	int id;
	u32 addr;
	u32 *syscall;

	id = find_free_hook();
	if(id < 0)
	{
		printf("No free API hooks left\n");
		return 0;
	}

	addr = libsFindExportByNid(uid, library, nid);
	if(addr)
	{
		syscall = find_syscall_addr(addr);
		if(syscall)
		{
			g_apihooks[id].syscall = syscall;
			g_apihooks[id].func = (void *) addr;
			g_apihooks[id].ret = ret;
			g_apihooks[id].sleep = sleep;
			strncpy(g_apihooks[id].param, format, APIHOOK_MAXPARAM);
			sprintf(g_apihooks[id].name, "Nid:0x%08X", nid);
			apiHookAddr(syscall, g_apihooks[id].hookfunc);

			return 1;
		}
	}

	return 0;
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
