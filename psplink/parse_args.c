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
#include <util.h>

int parse_args(char *in, char *out, int *argc, char **argv, int max_args)
{
	int ret = 1;
	char in_quote = 0;

	if((in == NULL) || (out == NULL) || (argc == NULL) || (argv == NULL) || (max_args <= 0))
	{
		printf("Error in parse_args, invalid arguments\n");
		return 0;
	}

	*argc = 0;

	/* Skip any leading white space */
	while(is_aspace(*in))
	{
		in++;
	}

	/* Check this isn't an empty string */
	if(*in == 0)
	{
		/* An empty string is technically valid */
		return 1;
	}

	/* Set first arg */
	argv[0] = out;
	*argc += 1;

	while((*in != 0) && (*argc < (max_args)))
	{
		/* Escape character */
		if(*in == '\\')
		{
			/* Skip the escape */
			in++;
			switch(*in)
			{
				case 'n': *out++ = 10;
						  in++;
						  break;
				case 'r': *out++ = 13;
						  in++;
						  break;
				case 0  : break; /* End of string */
				default : *out++ = *in++;
						  break;
			};
		}
		else
		{
			if((is_aspace(*in)) && (in_quote == 0))
			{
				*out++ = 0;
				while(is_aspace(*in))
				{
					in++;
				}
				if(*in != 0)
				{
					argv[*argc] = out;
					*argc += 1;
				}
			}
			else if((*in == '"') || (*in == '\''))
			{
				if(in_quote)
				{
					if(*in == in_quote)
					{
						in_quote = 0;
						in++;
					}
					else
					{
						*out++ = *in++;
					}
				}
				else
				{
					in_quote = *in;
					in++;
				}
			}
			else
			{
				*out++ = *in++;
			}
		}
	}

	if(in_quote)
	{
		printf("Missing matching quote %c\n", in_quote);
		return 0;
	}

	*out = 0;
	if(argv[*argc-1][0] == 0)
	{
		*argc -= 1;
	}

	argv[*argc] = NULL;

	return ret;
}
