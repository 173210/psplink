/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * debug.h - Debugger code for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#ifndef __DEBUGINC_H__
#define __DEBUGINC_H__

#include "../psplink_user/psplink_ex.h"

struct DebugEnv
{
	unsigned int flags;
	unsigned int DRCNTL;
	unsigned int IBC;
	unsigned int DBC;
	unsigned int IBA;
	unsigned int IBAM;
	unsigned int DBA;
	unsigned int DBAM;
	unsigned int DBD;
	unsigned int DBDM;
};


void debugPrintBPS(void);
int debugDeleteBp(int i);
int debugSetBP(unsigned int address);
void debugStep(int skip);
int debugHandleException(PsplinkRegBlock *pRegs);
void debugPrintHWRegs(void);
void debugEnableHW(void);
void debugDisableHW(void);
int debugHWEnabled(void);
int debugGetEnv(struct DebugEnv *env);
int debugSetEnv(struct DebugEnv *env);
void debugSetHWRegs(int argc, char **argv);

#endif
