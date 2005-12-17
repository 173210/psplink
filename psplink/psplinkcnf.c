/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psplinkcnf.c - Functions to manipulate the configuration file
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <string.h>
#include "psplink.h"
#include "psplinkcnf.h"

int is_aspace(int ch);

/* Seems the kernel's fdgetc is broken :/ */
static int fdgetc(struct ConfigFile *cnf)
{
	int ch = -1;

	if(cnf->read_size == 0)
	{
		int size;
		size = sceIoRead(cnf->fd, cnf->read_buf, MAX_BUFFER);
		if(size > 0)
		{
			cnf->read_size = size;
			cnf->read_pos = 0;
		}
		else
		{
			cnf->read_size = 0;
			cnf->read_pos = 0;
		}
	}

	if(cnf->read_pos < cnf->read_size)
	{
		ch = cnf->read_buf[cnf->read_pos++];
	}

	return ch;
}

/* As the kernel's fdgetc is broke so is fdgets */
static int fdgets(struct ConfigFile *cnf)
{
	int pos = 0;

	while(pos < (MAX_BUFFER-1))
	{
		int ch;

		ch = fdgetc(cnf);

		/* EOF */
		if(ch == -1)
		{
			break;
		}

		cnf->str_buf[pos++] = (char) ch;

		if(ch == '\n')
		{
			break;
		}
	}

	cnf->str_buf[pos] = 0;

	return pos;
}

int psplinkConfigOpen(const char *filename, struct ConfigFile *cnf)
{
	int iRet = 0;

	do
	{
		if(cnf == NULL)
		{
			Kprintf("Error, invalid configuration structure\n");
			break;
		}

		memset(cnf, 0, sizeof(struct ConfigFile));

		cnf->fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
		if(cnf->fd < 0)
		{
			Kprintf("Error, cannot open configuration file %s\n", filename);
			break;
		}

		iRet = 1;
	}
	while(0);

	return iRet;
}

void psplinkConfigClose(struct ConfigFile *cnf)
{
	do
	{
		if(cnf == NULL)
		{
			Kprintf("Error, invalid configuration structure\n");
			break;
		}

		if(cnf->fd < 0)
		{
			Kprintf("Error, invalid file descriptor\n");
			break;
		}

		sceIoClose(cnf->fd);
	}
	while(0);
}

void strip_whitesp(char *s)
{
	int start;
	int end;

	end = strlen(s);
	while(end > 0)
	{
		if(is_aspace(s[end-1]))
		{
			s[end-1] = 0;
		}
		else
		{
			break;
		}
	}

	start = 0;
	while(s[start])
	{
		if(is_aspace(s[start]))
		{
			start++;
		}
		else
		{
			break;
		}
	}

	if(start > 0)
	{
		int pos = 0;
		while(s[start])
		{
			s[pos++] = s[start++];
		}
		s[pos] = 0;
	}
}

const char* psplinkConfigReadNext(struct ConfigFile *cnf, const char **name)
{
	const char *pVal = NULL;

	do
	{
		if(cnf == NULL)
		{
			Kprintf("Error, invalid configuration structure\n");
			break;
		}

		if(cnf->fd < 0)
		{
			Kprintf("Error, invalid file descriptor\n");
			break;
		}

		while((pVal == NULL) && (fdgets(cnf)))
		{
			char *eq_pos;

			cnf->line++;
			strip_whitesp(cnf->str_buf);
			if((cnf->str_buf[0] == '#') || (cnf->str_buf[0] == 0))
			{
				continue;
			}

			eq_pos = strchr(cnf->str_buf, '=');
			if(eq_pos == NULL)
			{
				Kprintf("Error on line %d of configuration file. No '='\n", cnf->line);
				continue;
			}

			*eq_pos = 0;
			*name = cnf->str_buf;
			pVal = eq_pos + 1;
		}
	}
	while(0);

	return pVal;
}

