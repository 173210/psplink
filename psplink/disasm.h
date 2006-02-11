/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * disasm.h - PSPLINK disassembler code
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#ifndef __DISASM_H__
#define __DISASM_H__

#define DISASM_OPT_MAX       5
#define DISASM_OPT_HEXINTS   'x'
#define DISASM_OPT_MREGS     'r'
#define DISASM_OPT_SYMADDR   's'
#define DISASM_OPT_MACRO     'm'
#define DISASM_OPT_PRINTREAL 'p'

/* Enable hexadecimal integers for immediates */
void disasmSetHexInts(int hexints);
/* Enable mnemonic MIPS registers */
void disasmSetMRegs(int mregs);
/* Enable resolving of PC to a symbol if available */
void disasmSetSymAddr(int symaddr);
/* Enable instruction macros */
void disasmSetMacro(int macro);
void disasmSetPrintReal(int printreal);
void disasmSetOpts(const char *opts, int set);
const char *disasmGetOpts(void);
const char *disasmInstruction(unsigned int opcode, unsigned int PC, unsigned int *realregs);

/* Symbol resolver function type */
typedef int (*SymResolve)(unsigned int addr, char *output, int size);
/* Set the symbol resolver function */
void disasmSetSymResolver(SymResolve symresolver);

#endif
