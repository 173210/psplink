/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * debug.c - Debugger code for psplink.
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
#include "exception.h"
#include "util.h"
#include "psplink.h"

#define MAX_BPS 16
#define SW_BREAK_INST	0x0000000d

struct BreakPoint
{
	unsigned int address;
	unsigned int oldinst;
	int active;
};

static struct BreakPoint g_bps[MAX_BPS];
static struct BreakPoint g_stepbp[2];

/* Define some opcode stuff for the stepping function */
#define BEQ_OPCODE		0x4
#define BEQL_OPCODE		0x14
#define BGTZ_OPCODE 	0x7
#define BGTZL_OPCODE	0x17
#define BLEZ_OPCODE		0x6
#define BLEZL_OPCODE	0x16
#define BNE_OPCODE		0x5
#define BNEL_OPCODE		0x15

/* Reg Imm */
#define REGIMM_OPCODE 	0x1
#define BGEZ_OPCODE		0x1
#define BGEZAL_OPCODE	0x11
#define BGEZALL_OPCODE	0x13
#define BGEZL_OPCODE	0x3
#define BLTZ_OPCODE		0
#define BLTZAL_OPCODE	0x10
#define BLTZALL_OPCODE	0x12
#define BLTZL_OPCODE	0x2

#define J_OPCODE		0x2
#define JAL_OPCODE		0x3

/* Special opcode */
#define SPECIAL_OPCODE	0
#define JALR_OPCODE		0x9
#define JR_OPCODE		0x8
#define SYSCALL_OPCODE  0xc

/* Cop Branches (all the same) */
#define COP0_OPCODE		0x10
#define COP1_OPCODE		0x11
#define COP2_OPCODE		0x12
#define BCXF_OPCODE		0x100
#define BCXFL_OPCODE	0x102
#define BCXT_OPCODE		0x101
#define BCXTL_OPCODE	0x103

/* Generic step command , if skip then will try to skip over jals */
static void step_generic(PspDebugRegBlock *regs, int skip)
{
	u32 opcode;
	u32 epc;
	u32 targetpc;
	int branch = 0;
	int cond   = 0;
	int link   = 0;

	epc = regs->epc;
	targetpc = epc + 4;

	opcode = _lw(epc);

	switch(opcode >> 26)
	{
		case BEQ_OPCODE:
		case BEQL_OPCODE:
		case BGTZ_OPCODE:
		case BGTZL_OPCODE:
		case BLEZ_OPCODE:
		case BLEZL_OPCODE: 
		case BNE_OPCODE:
		case BNEL_OPCODE:
						    {
							   short ofs;

							   ofs = (short) (opcode & 0xffff);
							   cond = 1;
							   branch = 1;
							   targetpc += ofs * 4;
						   }
						   break;
		case REGIMM_OPCODE: {
								switch((opcode >> 16) & 0x1f)
								{
									case BGEZ_OPCODE:
									case BGEZAL_OPCODE:
									case BGEZALL_OPCODE:	
									case BGEZL_OPCODE:
									case BLTZ_OPCODE:
									case BLTZAL_OPCODE:
									case BLTZALL_OPCODE:
									case BLTZL_OPCODE: {
														   short ofs;

														   ofs = (short) (opcode & 0xffff);
														   cond = 1;
														   branch = 1;
														   targetpc += ofs * 4;
													   }
													   break;
								}
						    }
							break;
		case JAL_OPCODE:	link = 1;
		case J_OPCODE: {
							 u32 ofs;
							 
							 ofs = opcode & 0x3ffffff;
							 targetpc = (ofs << 2) | (targetpc & 0xf0000000);
							 branch = 1;
							 cond = 0;
						 }
						 break;
		case SPECIAL_OPCODE:
						 {
							 switch(opcode & 0x3f)
							 {
								 case JALR_OPCODE: link = 1;
								 case JR_OPCODE:
												 {
													 u32 rs;

													 rs = (opcode >> 21) & 0x1f;
													 targetpc = regs->r[rs];
													 branch = 1;
													 cond = 0;
												 }
												 break;
								 case SYSCALL_OPCODE:
												 targetpc = regs->r[31];
												 break;
							 };
						 }
						 break;
		case COP0_OPCODE:
		case COP1_OPCODE:
		case COP2_OPCODE:
						 {
							 switch((opcode >> 16) & 0x3ff)
							 {
								 case BCXF_OPCODE:
								 case BCXFL_OPCODE:
								 case BCXT_OPCODE:
								 case BCXTL_OPCODE:
									 				{
														short ofs;

														ofs = (short) (opcode & 0xffff);
														cond = 1;
														branch = 1;
														targetpc += ofs * 4;
													}
													break;
							 };
						 }
						 break;
	};

	if(link && skip)
	{
		g_stepbp[1].address = epc + 8;
		g_stepbp[1].oldinst = _lw(epc + 8);
		g_stepbp[1].active = 1;
		_sw(SW_BREAK_INST, epc + 8);
	}
	else if(branch)
	{
		g_stepbp[0].address = targetpc;
		g_stepbp[0].oldinst = _lw(targetpc);
		g_stepbp[0].active = 1;
		_sw(SW_BREAK_INST, targetpc);
			
		if((cond) && (targetpc != (epc + 8)))
		{
			g_stepbp[1].address = epc + 8;
			g_stepbp[1].oldinst = _lw(epc + 8);
			g_stepbp[1].active = 1;
			_sw(SW_BREAK_INST, epc + 8);
		}

	}
	else
	{
		g_stepbp[0].address = targetpc;
		g_stepbp[0].active = 1;
		g_stepbp[0].oldinst = _lw(targetpc);
		_sw(SW_BREAK_INST, targetpc);
	}
}

void debugStep(int skip)
{
	if(g_exception.exception)
	{
		step_generic(&g_exception.regs, skip);
		sceKernelDcacheWritebackInvalidateAll();
		sceKernelIcacheInvalidateAll();
		exceptionResume();
	}
	else
	{
		printf("Error, not in an exception\n");
	}
}

static struct BreakPoint *find_bp(unsigned int address)
{
	int i;

	/* Mask out top nibble so we match whether we end up in kmem,
	 * user mem or cached mem */
	address &= 0x0FFFFFFF;
	for(i = 0; i < MAX_BPS; i++)
	{
		if((g_bps[i].active) && ((g_bps[i].address & 0x0FFFFFFF) == address))
		{
			return &g_bps[i];
		}
	}

	return NULL;
}

static struct BreakPoint *find_freebp(void)
{
	int i;
	for(i = 0; i < MAX_BPS; i++)
	{
		if(!g_bps[i].active)
		{
			return &g_bps[i];
		}
	}

	return NULL;
}

int debugSetBP(unsigned int address)
{
	if(find_bp(address) == NULL)
	{
		struct BreakPoint *pBp;

		pBp = find_freebp();
		if(pBp != NULL)
		{
			pBp->oldinst = _lw(address);
			_sw(SW_BREAK_INST, address);
			pBp->address = address;
			pBp->active = 1;
			sceKernelDcacheWritebackInvalidateAll();
			sceKernelIcacheInvalidateAll();

			return 1;
		}
		else
		{
			printf("Error, could not find a free breakpoint\n");
		}
	}

	return 0;
}

int debugDeleteBp(int i)
{
	if((i >= 0) && (i < MAX_BPS))
	{
		if(g_bps[i].active)
		{
			_sw(g_bps[i].oldinst, g_bps[i].address);
			g_bps[i].active = 0;
			sceKernelDcacheWritebackInvalidateAll();
			sceKernelIcacheInvalidateAll();
		}
	}

	return 1;
}

void debugPrintBPS(void)
{
	int i;

	printf("Breakpoint List:\n");
	for(i = 0; i < MAX_BPS; i++)
	{
		if(g_bps[i].active)
		{
			printf("%-2d: Address %08X - Old Instruction %08X\n", i, g_bps[i].address, g_bps[i].oldinst);
		}
	}
}

int check_bp(unsigned int address)
{
	if((g_stepbp[0].active) && (g_stepbp[0].address == address))
	{
		return 1;
	}

	if((g_stepbp[1].active) && (g_stepbp[1].address == address))
	{
		return 1;
	}

	if(find_bp(address))
	{
		return 1;
	}

	return 0;
}

int debugHandleException(PspDebugRegBlock *pRegs)
{
	unsigned int address;
	struct BreakPoint *pBp;
	int ret = 0;

	address = pRegs->epc;

	if(check_bp(address))
	{
		/* Recover step break points */
		if(g_stepbp[0].active)
		{
			_sw(g_stepbp[0].oldinst, g_stepbp[0].address);
			g_stepbp[0].active = 0;
		}

		if(g_stepbp[1].active)
		{
			_sw(g_stepbp[1].oldinst, g_stepbp[1].address);
			g_stepbp[1].active = 0;
		}

		pBp = find_bp(address);
		if(pBp != NULL)
		{
			_sw(pBp->oldinst, pBp->address);
			pBp->active = 0;
		}

		sceKernelDcacheWritebackInvalidateAll();
		sceKernelIcacheInvalidateAll();
		printf("%s\n", PSPdis(address));

		ret = 1;
	}

	return ret;
}
