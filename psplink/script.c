/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * script.c - PSPLINK kernel module shell script code
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
#include <stdlib.h>
#include <string.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspumd.h>
#include <psputilsforkernel.h>
#include <pspsysmem_kernel.h>
#include <pspdisplay.h>
#include "memoryUID.h"
#include "psplink.h"
#include "psplinkcnf.h"
#include "parse_args.h"
#include "util.h"
#include "sio.h"
#include "bitmap.h"
#include "shell.h"

static int is_num(int ch)
{
	if((ch >= '0') && (ch <= '9'))
	{
		return 1;
	}

	return 0;
}

/* Go through the line and replace $1-$n with the argument strings */
static int process_arguments(char *line, int argc, char **argv, const char *lastmod)
{
	char tmpbuf[MAX_BUFFER];
	int inptr, outptr;

	inptr = 0;
	outptr = 0;
	while((line[inptr]) && (outptr < (MAX_BUFFER-1)))
	{
		/* Normal $ symbols should be escaped by putting them twice */
		if(line[inptr] == '$')
		{
			inptr += 1;
			if(is_num(line[inptr]))
			{
				int numsize;
				char num[16];
				int  val;
				/* Try and find the number the replace it */

				numsize = 0;
				while((is_num(line[inptr])) && (numsize < (sizeof(num)-1)))
				{
					num[numsize++] = line[inptr++];
				}
				num[numsize] = 0;
				val = atoi(num);
				if(val < argc)
				{
					char *str;
					str = argv[val];
					while((*str) && (outptr < (MAX_BUFFER-1)))
					{
						tmpbuf[outptr++] = *str++;
					}
				}
			}
			else if(line[inptr] == '!')
			{
				const char *str;

				inptr++;
				str = lastmod;
				while((*str) && (outptr < (MAX_BUFFER-1)))
				{
					tmpbuf[outptr++] = *str++;
				}
			}
			else
			{
				tmpbuf[outptr++] = line[inptr++];
			}
		}
		else
		{
			tmpbuf[outptr++] = line[inptr++];
		}
	}

	tmpbuf[outptr] = 0;
	strcpy(line, tmpbuf);

	return 1;
}

/* Run a shell script (with possible arguments */
int scriptRun(const char *filename, int argc, char **argv, const char *lastmod, int print)
{
	PspFile file;
	char line[MAX_BUFFER];
	int ret = CMD_OK;
	int i = 0;

	if(openfile(filename, &file))
	{
		while(fdgets(&file, line, 1024))
		{
			i++;
			strip_whitesp(line);
			/* Fill in arguments */
			if(strcmp(line, "echo on") == 0)
			{
				print = 1;
				continue;
			}
			else if(strcmp(line, "echo off") == 0)
			{
				print = 0;
				continue;
			}
			else if(!process_arguments(line, argc, argv, lastmod))
			{
				printf("Error processing arguments on line %d\n", i);
				ret = CMD_ERROR;
				break;
			}

			if(print)
			{
				printf("line %d: %s\n", i, line);
			}

			ret = shellParse(line);
			if(ret != CMD_OK)
			{
				printf("Error in command on line %d\n", i);
				break;
			}
		}
		closefile(&file);
	}

	return ret;
}
