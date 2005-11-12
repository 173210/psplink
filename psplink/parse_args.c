/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * parse_args.c - PSPLINK argument parser code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#include <pspkernel.h>
#include <stdio.h>
#include <string.h>

static char *find_char(char *str, const char *chs)
{
	char *ret = NULL;

	while(*str != 0)
	{
		const char *find;

		find = chs;
		while(*find != 0)
		{
			if(*str == *find)
			{
				ret = str;
				break;
			}
			find++;
		}

		if(ret != NULL)
		{
			break;
		}

		str++;
	}

	return ret;
}

/* Very simple arg parser, does not support escaped characters */
int parse_args(char *args, int *argc, char **argv, int max_args)
{
	int ret = 1;

	if((args == NULL) || (argc == NULL) || (argv == NULL) || (max_args <= 0))
	{
		Kprintf("Error in parse_args, invalid arguments\n");
		return 0;
	}

	*argc = 0;

	while((*args != 0) && (*argc < max_args))
	{
		if((*args == '"') || (*args == '\''))
		{
			char *next_quote;
			
			next_quote = strchr(args + 1, *args);
			if(next_quote == NULL)
			{
				Kprintf("Missing closing quote %c\n", *args);
				ret = 0;
				break;
			}
			else
			{
				*next_quote = 0;
				argv[*argc] = args + 1;
				*argc += 1;
				args = next_quote + 1;
			}
		}
		else
		{
			char *endp;

			endp = find_char(args, " \"'");
			if(endp != NULL)
			{
				if((*endp == '"') || (*endp == '\''))
				{
					Kprintf("Invalid quote in argument\n");
					ret = 0;
					break;
				}
				*endp = 0;
				argv[*argc] = args;
				args = endp + 1;
				*argc += 1;
			}
			else
			{
				argv[*argc] = args;
				*argc += 1;
				break; /* Exit */
			}
		}
	}

	return ret;
}
