/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * symbols.h - Symbol managment code for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#ifndef __SYMBOLS_H__
#define __SYMBOLS_H__

#include <pspdebug.h>

struct SymfileEntry
{
	const char *name;
	u32 addr;
	u32 size;
} __attribute__((packed));

const struct SymfileEntry* symbolFindByAddress(unsigned int addr, unsigned int *baseaddr);
const char *symbolFindNameByAddress(unsigned int addr);
unsigned int symbolFindByName(const char *name);
void symbolPrintLoadList(void);
void symbolPrintSymbols(const char *modname);
int symbolLoadSymbols(const char *file);
int symbolDeleteSymbols(const char *modname);
void symbolDeleteAll(void);

#endif
