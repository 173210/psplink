/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * decodeaddr.c - PSPLINK kernel module decode memory address code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#include <pspkernel.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "decodeaddr.h"
#include "exception.h"
#include "util.h"

struct mem_entry
{
	u32 addr;
	s32 size;
	u32 attrib;
	const char *desc;
};

static struct mem_entry g_memareas[] = 
{
	{ 0x08800000, (24 * 1024 * 1024), MEM_ATTRIB_ALL, "User memory" },
	{ 0x48800000, (24 * 1024 * 1024), MEM_ATTRIB_ALL, "User memory (uncached)" },
	{ 0x88000000, (4 * 1024 * 1024), MEM_ATTRIB_ALL, "Kernel memory (low)" },
	{ 0xC8000000, (4 * 1024 * 1024), MEM_ATTRIB_ALL, "Kernel memory (low uncached)" },
	/* Don't use the following 2 on a 1.5, just crashes the psp */
//	{ 0x88400000, (4 * 1024 * 1024), MEM_ATTRIB_ALL, "Kernel memory (mid v1.0 only)" },
//	{ 0xC8400000, (4 * 1024 * 1024), MEM_ATTRIB_ALL, "Kernel memory (mid v1.0 only uncached)" },
	{ 0x88800000, (24 * 1024 * 1024), MEM_ATTRIB_ALL, "Kernel memory (high)" },
	{ 0xC8800000, (24 * 1024 * 1024), MEM_ATTRIB_ALL, "Kernel memory (high uncached)" },
	{ 0x04000000, (2 * 1024 * 1024), MEM_ATTRIB_ALL, "VRAM" },
	{ 0x44000000, (2 * 1024 * 1024), MEM_ATTRIB_ALL, "VRAM (uncached)" },
	{ 0, 0, 0, NULL }
};

enum Operator
{
	OP_START,
	OP_NONE,
	OP_PLUS,
	OP_MINUS,
	OP_AND,
	OP_OR,
	OP_XOR,
};

static unsigned int do_op(enum Operator op, int not, unsigned int left, unsigned int right)
{
	unsigned ret = 1;
	int i;

	for(i = 0; i < not; i++)
	{
		right = ~right;
	}

	switch(op)
	{
		case OP_PLUS: ret = left + right;
					  break;
		case OP_MINUS: ret = left - right;
					   break;
		case OP_AND: ret = left & right;
					 break;
		case OP_OR:  ret = left | right;
					 break;
		case OP_XOR: ret = left ^ right;
					 break;
		default:     ret = right;
					 break;
	};

	return ret;
}

static int deref_addr(unsigned int *val, int deref)
{
	int size;
	int i;

	for(i = 0; i < deref; i++)
	{
		if(*val & 0x3)
		{
			printf("Error, unaligned address when dereferencing\n");
			return 0;
		}

		size = memValidate(*val, MEM_ATTRIB_READ | MEM_ATTRIB_WORD);
		if(size < sizeof(unsigned int *))
		{
			printf("Error, invalid memory address when dereferencing %08X\n", *val);
			return 0;
		}

		*val = _lw(*val);
	}

	return 1;
}

static int get_modaddr(const char *name, unsigned int *val)
{
	SceKernelModuleInfo info;
	SceModule *pMod;
	SceUID uid = 0;
	int ret = 0;

	pMod = sceKernelFindModuleByName(name);
	if(pMod == NULL)
	{
		char *endp;

		uid = strtoul(name, &endp, 16);
		if(*endp != 0)
		{
			printf("Error, invalid module name %s\n", name);
			return 0;
		}
	}
	else
	{
		uid = pMod->modid;
	}

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	pspDebugSioDisableKprintf();
	ret = g_QueryModuleInfo(uid, &info);
	pspDebugSioEnableKprintf();
	if(ret < 0)
	{
		printf("Error, could not get module info\n");
		return 0;
	}

	*val = info.text_addr;

	return 1;
}

static int get_threadaddr(const char *name, unsigned int *val)
{
	SceKernelThreadInfo info;
	SceUID uid;

	if(pspSdkReferThreadStatusByName(name, &uid, NULL))
	{
		char *endp;

		uid = strtoul(name, &endp, 16);
		if(*endp != 0)
		{
			printf("Error, invalid thread name %s\n", name);
			return 0;
		}
	}

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if(sceKernelReferThreadStatus(uid, &info))
	{
		printf("Error, could not get thread info\n");
		return 0;
	}

	*val = (u32) info.entry;

	return 1;
}

static int parse_line(char *line, unsigned int *val)
{
	enum Operator op = OP_START;
	int deref = 0;
	int not = 0;
	*val = 0;

	while(*line)
	{
		unsigned int temp;

		if(is_aspace(*line))
		{
			line++;
			continue;
		}
		else if(*line == '*')
		{
			deref++;
			line++;
			continue;
		}
		else if(*line == '~')
		{
			not++;
			line++;
			continue;
		}
		else if(*line == '$')
		{
			char buf[16];
			int pos;
			u32 *reg;

			pos = 0;
			line++;
			while((pos < 15) && (is_alnum(*line)))
			{
				buf[pos++] = *line++;
			}
			buf[pos] = 0;

			reg = exceptionGetReg(buf);
			if(reg == NULL)
			{
				printf("Unknown register '%s'\n", buf);
				return 0;
			}

			temp = *reg;
		}
		else if(*line == '@') /* Module name */
		{
			char *endp;
			line++;

			endp = strchr(line, '@');
			if(endp == NULL)
			{
				printf("Error, no matching '@' for module name\n");
				return 0;
			}

			*endp = 0;
			if(!get_modaddr(line, &temp))
			{
				return 0;
			}

			line = endp+1;
		}
		else if(*line == '%') /* Thread name */
		{
			char *endp;
			line++;

			endp = strchr(line, '%');
			if(endp == NULL)
			{
				printf("Error, no matching '%%' for thread name\n");
				return 0;
			}

			*endp = 0;
			/* Decode the module name */
			if(!get_threadaddr(line, &temp))
			{
				return 0;
			}

			line = endp+1;
		}
		else if(*line == '(')
		{
			/* Scan for end of brackets, NUL terminate and pass along */
			char *pos;
			int depth = 1;

			pos = ++line;
			while(*pos)
			{
				if(*pos == '(')
				{
					depth++;
				}
				else if(*pos == ')')
				{
					depth--;
					if(depth == 0)
					{
						break;
					}
				}
				else
				{
					/* Do Nothing */
				}

				pos++;
			}

			if(depth != 0)
			{
				printf("Error, unmatched bracket\n");
				return 0;
			}

			*pos = 0;
			if(!parse_line(line, &temp))
			{
				return 0;
			}
			line = pos + 1;
		}
		else if(is_hex(*line))
		{
			char *endp;

			if(op == OP_NONE)
			{
				printf("Error, cannot place two numbers together\n");
				return 0;
			}
			/* strtoul the value */
			temp = strtoul(line, &endp, 0);
			line = endp;
		}
		else 
		{
			if(op != OP_NONE)
			{
				printf("Invalid character %c\n", *line);
				return 0;
			}

			/* Do switch for possible operators */
			switch(*line)
			{
				case '+': op = OP_PLUS;
						  break;
				case '-': op = OP_MINUS;
						  break;
				case '&': op = OP_AND;
						  break;
				case '|': op = OP_OR;
						  break;
				case '^': op = OP_XOR;
						  break;
				default : printf("Invalid character %c\n", *line);
						  return 0;
			};
			line++;
			continue;
		}

		/* Do operation */
		if(deref > 0)
		{
			if(deref_addr(&temp, deref) == 0)
			{
				return 0;
			}
		}
		deref = 0;
		*val = do_op(op, not, *val, temp);
		not = 0;
		op = OP_NONE;
	}
	

	return 1;
}

int memDecode(const char *line, u32 *val)
{
	char line_buf[1024];

	strncpy(line_buf, line, 1023);
	line_buf[1023] = 0;

	return parse_line(line_buf, val);
}

int memValidate(u32 addr, u32 attrib)
{
	const struct mem_entry *entry;
	int size_left = 0;

	entry = g_memareas;

	while(entry->size != 0)
	{
		if((addr >= entry->addr) && (addr < (entry->addr + (u32) entry->size)))
		{
			/* Only pass through areas with valid attributes (e.g. write or execute) */
			if((entry->attrib & attrib) == attrib)
			{
				size_left = entry->size - (int) (addr - entry->addr);
			}
			break;
		}

		entry++;
	}

	return size_left;
}

void memPrintRegions(void)
{
	int i;
	printf("Memory Regions:\n");
	i = 0;
	while(g_memareas[i].addr)
	{
		printf("Region %d: Base %08X - Size %08X - %s\n", i,
				g_memareas[i].addr, g_memareas[i].size, g_memareas[i].desc);
		i++;
	}
}
