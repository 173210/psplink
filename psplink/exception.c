/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * exception.c - Exception handler for psplink.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "exception.h"
#include "util.h"
#include "psplink.h"
#include "debug.h"

struct ExceptionContext g_exception;

/* Mnemonic register names */
static const char regName[32][5] =
{
    "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", 
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

/* Taken from the ps2, might not be 100% correct */
static const char *codeTxt[32] = 
{
    "Interrupt", "TLB modification", "TLB load/inst fetch", "TLB store",
    "Address load/inst fetch", "Address store", "Bus error (instr)", 
    "Bus error (data)", "Syscall", "Breakpoint", "Reserved instruction", 
    "Coprocessor unusable", "Arithmetic overflow", "Unknown 14",
	"Unknown 15", "Unknown 16", "Unknown 17", "Unknown 18", "Unknown 19",
	"Unknown 20", "Unknown 21", "Unknown 22", "Unknown 23", "Unknown 24",
	"Unknown 25", "Unknown 26", "Unknown 27", "Unknown 28", "Unknown 29",
	"Unknown 31"
};

u32 *exceptionGetReg(const char *reg)
{
	if(strcmp(reg, "epc") == 0)
	{
		return &g_exception.regs.epc;
	}
	else 
	{
		int reg_loop;

		for(reg_loop = 0; reg_loop < 32; reg_loop++)
		{
			if(strcmp(regName[reg_loop], reg) == 0)
			{
				return &g_exception.regs.r[reg_loop];
			}
		}
	}

	return NULL;
}

void exceptionPrintCPURegs(u32 *pRegs)
{
	int i;

	printf("%s:0x00000000 %s:0x%08X %s:0x%08X %s:0x%08X\n", regName[0], 
			regName[1], pRegs[1], regName[2], 
			pRegs[2], regName[3], pRegs[3]);
	for(i = 4; i < 32; i+=4)
	{
		printf("%s:0x%08X %s:0x%08X %s:0x%08X %s:0x%08X\n", regName[i], pRegs[i],
				regName[i+1], pRegs[i+1], regName[i+2], 
				pRegs[i+2], regName[i+3], pRegs[i+3]);
	}
}

void exceptionPrint(void)
{
	if(g_exception.exception)
	{
		printf("Exception - %s\n", codeTxt[(g_exception.regs.cause >> 2) & 31]);
		printf("Thread ID - 0x%08X\n", g_exception.thid);
		printf("Th Name   - %s\n", g_exception.threadname);
		printf("Module ID - 0x%08X\n", g_exception.modid);
		printf("Mod Name  - %s\n", g_exception.modulename);
		printf("EPC       - 0x%08X\n", g_exception.regs.epc);
		printf("Cause     - 0x%08X\n", g_exception.regs.cause);
		printf("Status    - 0x%08X\n", g_exception.regs.status);
		printf("BadVAddr  - 0x%08X\n", g_exception.regs.badvaddr);
		exceptionPrintCPURegs(g_exception.regs.r);
	}
	else
	{
		printf("No exception occurred\n");
	}
}

void exceptionPrintFPURegs(float *pFpu)
{
	int i;

	pspSdkDisableFPUExceptions();

	for(i = 0; i < 32; i+=2)
	{
		char left[64], right[64];

		f_cvt(pFpu[i], left, sizeof(left), 6, MODE_GENERIC);
		f_cvt(pFpu[i+1], right, sizeof(right), 6, MODE_GENERIC);
		printf("fpr%02d: %-20s - fpr%02d: %-20s\n", i, left, i+1, right);
	}
}

void exceptionFpuPrint(void)
{
	if(g_exception.exception)
	{
		exceptionPrintFPURegs(g_exception.regs.fpr);
	}
	else
	{
		printf("No exception occurred\n");
	}
}

void psplinkHandleException(PspDebugRegBlock *regs)
{
	u32 k1;
	SceModule *pMod;
	SceKernelModuleInfo mod;
	SceKernelThreadInfo thread;
	u32 addr;

	k1 = psplinkSetK1(0);

	memset(&g_exception, 0, sizeof(g_exception));
	memcpy(&g_exception.regs, regs, sizeof(*regs));
	g_exception.exception = 1;
	g_exception.thid = sceKernelGetThreadId();
	if(!sceKernelReferThreadStatus(g_exception.thid, &thread))
	{
		strncpy(g_exception.threadname, thread.name, 31);
		g_exception.threadname[31] = 0;
	}

	if((g_exception.regs.epc < 0x88000000) || (g_exception.regs.epc > 0x88400000))
	{
		addr = g_exception.regs.epc & 0x7FFFFFFF;
	}
	else
	{
		addr = g_exception.regs.epc;
	}

	pMod = sceKernelFindModuleByAddress(addr);
	if(pMod)
	{
		g_exception.modid = pMod->modid;
		mod.size = sizeof(mod);
		pspDebugSioDisableKprintf();
		if(!g_QueryModuleInfo(g_exception.modid, &mod))
		{
			strncpy(g_exception.modulename, mod.name, 31);
			g_exception.modulename[31] = 0;
		}
		pspDebugSioEnableKprintf();
	}
	else
	{
		g_exception.modid = -1;
	}

	/* If this was not an exception caused by us then just dump the registers to screen */
	if(!debugHandleException(&g_exception.regs))
	{
		exceptionPrint();
	}
	psplinkSetK1(k1);

	/* Sleep thread */
	sceKernelSleepThread();

	memcpy(regs, &g_exception.regs, sizeof(*regs));
}

void exceptionResume(void)
{
	if(g_exception.exception)
	{
		g_exception.exception = 0;
		sceKernelWakeupThread(g_exception.thid);
	}
}

void exceptionInit(void)
{
	memset(&g_exception, 0, sizeof(g_exception));
}
