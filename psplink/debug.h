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

#include <pspdebug.h>

void debugPrintBPS(void);
int debugDeleteBp(int i);
int debugSetBP(unsigned int address);
void debugStep(int skip);
int debugHandleException(PspDebugRegBlock *pRegs);

#endif
