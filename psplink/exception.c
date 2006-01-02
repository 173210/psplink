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
#include <stdio.h>
#include "psplink.h"

void _GdbTrapEntry(PspDebugRegBlock *regs);

int g_debuggermode = 0;

/* Mnemonic register names */
static const unsigned char regName[32][5] =
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

/* Dump an exception to sio */
void DumpException(PspDebugRegBlock *regs)
{
	int i;

	printf("Exception - %s\n", codeTxt[(regs->cause >> 2) & 31]);
	printf("Thread ID - %08X\n", sceKernelGetThreadId());
	printf("EPC       - %08X\n", regs->epc);
	printf("Cause     - %08X\n", regs->cause);
	printf("Status    - %08X\n", regs->status);
	printf("BadVAddr  - %08X\n", regs->badvaddr);
	for(i = 0; i < 32; i+=4)
	{
		printf("%s:%08X %s:%08X %s:%08X %s:%08X\n", regName[i], regs->r[i],
				regName[i+1], regs->r[i+1], regName[i+2], regs->r[i+2], regName[i+3], regs->r[i+3]);
	}
}

int psplinkHandleException(PspDebugRegBlock *regs)
{
	u32 k1;

	DEBUG_PRINTF("Resume Pointer - debugger %d\n", g_debuggermode);
	k1 = psplinkSetK1(0);
	if(g_debuggermode)
	{
		/* Do GDB stub */
		_GdbTrapEntry(regs);
	}
	else
	{
		DumpException(regs);
		psplinkSetK1(k1);
		return 0;
	}

	return 1;
}
