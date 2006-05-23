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
#include <pspexception.h>
#include <stdio.h>
#include <string.h>
#include "exception.h"
#include "util.h"
#include "psplink.h"
#include "debug.h"
#include "sio.h"

struct PsplinkContext *g_currex = NULL;
struct PsplinkContext *g_list = NULL;

#define ROW_NUM(x) ((x) / 16)
#define COL_NUM(x) (((x) / 4) & 3)
#define MAT_NUM(x) ((x) & 3)

#define FPU_EXCEPTION 15

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
    "Coprocessor unusable", "Arithmetic overflow", "Unknown 13", "Unknown 14", 
	"FPU Exception", "Unknown 16", "Unknown 17", "Unknown 18",
	"Unknown 20", "Unknown 21", "Unknown 22", "Unknown 23", 
	"Unknown 24", "Unknown 25", "Unknown 26", "Unknown 27", 
	"Unknown 28", "Unknown 29", "Unknown 30", "Unknown 31"
};

const char codeFpu[6] = { 'I', 'U', 'O', 'Z', 'V', 'E' };
const char codeDebug[7] = { 'X', 'S', 'B', '?', '?', 'I', 'D' };

int psplinkRegisterExceptions(void *def, void *debug, void *ctx)
{
	g_list = ctx;
	sceKernelRegisterDefaultExceptionHandler(def);
	sceKernelRegisterPriorityExceptionHandler(24, 1, debug);

	return 0;
}

/* Get a pointer to a register based on its name */
u32 *exceptionGetReg(const char *reg)
{
	if(strcmp(reg, "epc") == 0)
	{
		return &g_currex->regs.epc;
	}
	else if(strcmp(reg, "fsr") == 0)
	{
		return &g_currex->regs.fsr;
	}
	else 
	{
		int reg_loop;

		for(reg_loop = 0; reg_loop < 32; reg_loop++)
		{
			if(strcmp(regName[reg_loop], reg) == 0)
			{
				return &g_currex->regs.r[reg_loop];
			}
		}
	}

	return NULL;
}

/* Print the cpu registers, pointer should contain a dummy entry
 * for zero as it is relatively addressed */
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

static const char *exception_cause(struct PsplinkContext *pCtx)
{
	static char excause[40];

	if(pCtx->regs.type == PSPLINK_EXTYPE_NORMAL)
	{
		if(((pCtx->regs.cause >> 2) & 31) == FPU_EXCEPTION)
		{
			int i;
			u32 fpu;
			char *end;

			strcpy(excause, codeTxt[(pCtx->regs.cause >> 2) & 31]);
			strcat(excause, " (");
			end = excause + strlen(excause);

			fpu = pCtx->regs.fsr >> 12;
			for(i = 0; i < 6; i++)
			{
				if((fpu >> i) & 1)
				{
					*end++ = codeFpu[i];
				}
			}
			*end++ = ')';
			*end = 0;
		}
		else
		{
			strcpy(excause, codeTxt[(pCtx->regs.cause >> 2) & 31]);
		}
	}
	else
	{
		int i;
		char *end;

		strcpy(excause, "DEBUG");
		strcat(excause, " (");
		end = excause + strlen(excause);
		for(i = 0; i < 7; i++)
		{
			if(pCtx->drcntl & (1 << (i + 6)))
			{
				*end++ = codeDebug[i];
			}
		}
		*end++ = ')';
		*end = 0;
	}

	return excause;
}

/* Print the current exception */
void exceptionPrint(int ex)
{
	SceModule *pMod;
	SceKernelModuleInfo mod;
	SceKernelThreadInfo thread;
	u32 addr;
	struct PsplinkContext *ctx = NULL;

	if((ex >= 0) && (ex < PSPLINK_MAX_CONTEXT))
	{
		if((g_list) && (g_list[ex].valid))
		{
			ctx = &g_list[ex];
		}
	}
	else
	{
		ctx = g_currex;
	}

	if(ctx)
	{
		printf("Exception - %s\n", exception_cause(ctx));
		printf("Thread ID - 0x%08X\n", ctx->thid);

		memset(&thread, 0, sizeof(thread));
		thread.size = sizeof(thread);
		if(!sceKernelReferThreadStatus(ctx->thid, &thread))
		{
			printf("Th Name   - %s\n", thread.name);
		}

		if((ctx->regs.epc < 0x88000000) || (ctx->regs.epc > 0x88400000))
		{
			addr = ctx->regs.epc & 0x7FFFFFFF;
		}
		else
		{
			addr = ctx->regs.epc;
		}

		pMod = sceKernelFindModuleByAddress(addr);
		if(pMod)
		{
			printf("Module ID - 0x%08X\n", pMod->modid);
			memset(&mod, 0, sizeof(mod));
			mod.size = sizeof(mod);
			sioDisableKprintf();
			if(!g_QueryModuleInfo(pMod->modid, &mod))
			{
				printf("Mod Name  - %s\n", mod.name);
			}
			sioEnableKprintf();
		}

		printf("EPC       - 0x%08X\n", ctx->regs.epc);
		if(g_currex->regs.type == PSPLINK_EXTYPE_NORMAL)
		{
			printf("Cause     - 0x%08X\n", ctx->regs.cause);
			printf("BadVAddr  - 0x%08X\n", ctx->regs.badvaddr);
		}
		else
		{
			printf("DRCNTL    - 0x%08X\n", ctx->drcntl);
		}

		printf("Status    - 0x%08X\n", ctx->regs.status);
		exceptionPrintCPURegs(ctx->regs.r);
	}
	else
	{
		printf("No exception occurred\n");
	}
}

void exceptionList(void)
{
	int i;

	if(g_list)
	{
		for(i = 0; i < PSPLINK_MAX_CONTEXT; i++)
		{
			if(g_list[i].valid)
			{
				printf("Exception %-2d: EPC 0x%08X, Cause %s\n", i, g_list[i].regs.epc, 
						exception_cause(&g_list[i]));
			}
		}
	}
	else
	{
		printf("No exception handler registered\n");
	}
}

void exceptionPrintFPURegs(float *pFpu, unsigned int fsr, unsigned int fir)
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
	printf("fsr: %08X   - fir %08X\n", fsr, fir);
}

void exceptionFpuPrint(int ex)
{
	struct PsplinkContext *ctx = NULL;

	if((ex >= 0) && (ex < PSPLINK_MAX_CONTEXT))
	{
		if((g_list) && (g_list[ex].valid))
		{
			ctx = &g_list[ex];
		}
	}
	else
	{
		ctx = g_currex;
	}

	if(ctx)
	{
		if(ctx->regs.status & 0x20000000)
		{
			exceptionPrintFPURegs(ctx->regs.fpr, ctx->regs.fsr,
					ctx->regs.fir);
		}
		else
		{
			printf("FPU not enabled in context\n");
		}
	}
	else
	{
		printf("No exception occurred\n");
	}
}

void exceptionPrintVFPURegs(float *pFpu, int mode)
{
	int i;

	pspSdkDisableFPUExceptions();

	if(mode == VFPU_PRINT_SINGLE)
	{
		for(i = 0; i < 128; i+=2)
		{
			char left[64], right[64];

			f_cvt(pFpu[i], left, sizeof(left), 6, MODE_GENERIC);
			f_cvt(pFpu[i+1], right, sizeof(right), 6, MODE_GENERIC);
			printf("S%d%d%d: %-20s - S%d%d%d: %-20s\n", 
					MAT_NUM(i), COL_NUM(i), ROW_NUM(i), left, 
					MAT_NUM(i+1), COL_NUM(i+1), ROW_NUM(i+1),  right);
		}
	}
}

void exceptionVfpuPrint(int ex, int mode)
{
	struct PsplinkContext *ctx = NULL;

	if((ex >= 0) && (ex < PSPLINK_MAX_CONTEXT))
	{
		if((g_list) && (g_list[ex].valid))
		{
			ctx = &g_list[ex];
		}
	}
	else
	{
		ctx = g_currex;
	}

	if(ctx)
	{
		if(ctx->regs.status & 0x40000000)
		{
			exceptionPrintVFPURegs(ctx->regs.vfpu, mode);
		}
		else
		{
			printf("VFPU not enabled in context\n");
		}
	}
	else
	{
		printf("No exception occurred\n");
	}
}

void psplinkHandleException(struct PsplinkContext *ctx)
{
	u32 k1;

	k1 = psplinkSetK1(0);

	g_currex = ctx;

	/* If this was not an exception caused by us then just dump the registers to screen */
	if(!debugHandleException(&g_currex->regs))
	{
		exceptionPrint(-1);
	}
	psplinkSetK1(k1);

	/* Sleep thread */
	sceKernelSleepThread();
}

void exceptionResume(void)
{
	if(g_currex)
	{
		sceKernelWakeupThread(g_currex->thid);
		g_currex = NULL;
	}
}

void exceptionSetCtx(int ex)
{
	if((ex >= 0) && (ex < PSPLINK_MAX_CONTEXT))
	{
		if((g_list) && (g_list[ex].valid))
		{
			g_currex = &g_list[ex];
		}
	}
}

void exceptionInit(void)
{
}
