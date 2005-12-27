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

int decode_hex(const char *str, unsigned char *ch)
{
	int i;

	*ch = 0;
	for(i = 0; i < 2; i++)
	{
		if(!is_hex(str[i]))
		{
			break;
		}
		*ch = *ch << 4;
		*ch = *ch | hex_to_int(str[i]);
	}
	if(i == 0)
	{
		printf("Missing following hex characters\n");
		return 0;
	}
	if(*ch == 0)
	{
		printf("Invalid hex character (not allowed NULs)\n");
		return 0;
	}

	return i;
}

int decode_oct(const char *str, unsigned char *ch)
{
	int i;

	*ch = 0;
	for(i = 0; i < 4; i++)
	{
		if(!is_oct(str[i]))
		{
			break;
		}
		*ch = *ch << 3;
		*ch = *ch | oct_to_int(str[i]);
	}
	if(i == 0)
	{
		printf("Missing following octal characters\n");
		return 0;
	}
	if(*ch == 0)
	{
		printf("Invalid octal character (not allowed NULs)\n");
		return 0;
	}

	return i;
}

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
				case '0': /* Octal */
						  {
							  int i;
							  unsigned char ch;
							  in++;
							  i = decode_oct(in, &ch);
							  if(i == 0)
							  {
								  return 0;
							  }
							  in += i;
							  *out++ = ch;
						  }
						  break;
				case 'x': /* Hexadecimal */
						  {
							  int i;
							  unsigned char ch;
							  in++;
							  i = decode_hex(in, &ch);
							  if(i == 0)
							  {
								  return 0;
							  }
							  in += i;
							  *out++ = ch;
						  }
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
