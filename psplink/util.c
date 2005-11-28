/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * util.c - util functions for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
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

#include "util.h"

int is_aspace(int ch)
{
	if((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'))
	{
		return 1;
	}

	return 0;
}

/* Normalise the path, remove . and .. directories, will ignore anything at the end with no dir slash */
static int normalize_path(char *path)
{
	char *last_dir = NULL;
	char *curr_pos;
	int ret = 1;

	/* Can't start with an absolute path */
	if(*path == '/')
	{
		ret = 0;
	}
	else
	{
		curr_pos = strchr(path, '/');
		while(curr_pos != NULL)
		{
			if(last_dir != NULL)
			{
				if(strncmp(last_dir, "/.", curr_pos - last_dir) == 0)
				{
					strcpy(last_dir, curr_pos);
					curr_pos = last_dir;
				}
				else if(strncmp(last_dir, "/..", curr_pos - last_dir) == 0)
				{
					char *last_pos;
					/* Find the last directory slash from last_dir */
					last_pos = last_dir - 1;
					while(last_pos > path)
					{
						if(*last_pos == '/')
						{
							break;
						}
						last_pos--;
					}

					if(last_pos > path)
					{
						last_dir = last_pos;
					}

					strcpy(last_dir, curr_pos);
					curr_pos = last_dir;
				}
				else
				{
					/* Ignore */
				}
			}

			last_dir = curr_pos;
			curr_pos = strchr(curr_pos + 1, '/');
		}
	}

	return ret;
}

int handlepath(char *currentdir, char *relative, char *path, int type, int valid)
{
	int len, fd;

	/* Strip whitespace and append a final slash */
	path[0] = 0;
	if(strchr(relative, ':') == NULL)
	{
		if(relative[0] == '/')
		{
			int currdir_pos = 0;
			int path_pos = 0;
			while(currentdir[currdir_pos] != 0)
			{
				path[path_pos] = currentdir[currdir_pos];
				if(currentdir[currdir_pos] == ':')
				{
					path[path_pos + 1] = 0;
					break;
				}
				currdir_pos++;
				path_pos++;
			}
		}
		else
		{
			/* relative directory */
			strcpy(path, currentdir);
		}
	}

	strcat(path, relative);
	len = strlen(path);
	while((len > 0) && (is_aspace(path[len-1])))
	{
		path[len-1] = 0;
		len--;
	}

	/* Very unsafe, but still */
	if(type == TYPE_DIR && path[len-1] != '/') {
		path[len] = '/';
		path[len+1] = 0;
	} else if(type == TYPE_FILE && path[len] == '/') {
		path[len] = 0;
	}

	if(normalize_path(path) == 0)
		return 0;

	if(valid) {
		if(type == TYPE_DIR) {
			if((fd = sceIoDopen(path)) < 0) {
				/* Invalid Directory */
				return 0;
			} else {
				sceIoDclose(fd);
			}
		} else if(type == TYPE_FILE) {
			if((fd = sceIoOpen(path, PSP_O_RDONLY, 0777)) < 0) {
				/* Invalid File */
				return 0;
			} else {
				sceIoClose(fd);
			}
		} else {
			printf("unable to validate ether type\n");
			return 0;
		}
	}

	return 1;
}

/* Make the character upper case */
char upcase(char ch)
{
	if((ch >= 'a') && (ch <= 'z'))
	{
		ch ^= (1 << 5);
	}

	return ch;
}

int build_bootargs(char *args, const char *bootfile, const char *execfile, int argc, char **argv)
{
	int loc = 0;
	int i;

	strcpy(args, bootfile);
	loc += strlen(bootfile) + 1;
	if(execfile != NULL)
	{
		strcpy(&args[loc], execfile);
		loc += strlen(execfile) + 1;
		for(i = 0; i < argc; i++)
		{
			strcpy(&args[loc], argv[i]);
			loc += strlen(argv[i]) + 1;
		}
	}

	return loc;
}

int build_args(char *args, const char *execfile, int argc, char **argv)
{
	int loc = 0;
	int i;

	strcpy(args, execfile);
	loc += strlen(execfile) + 1;
	for(i = 0; i < argc; i++)
	{
		strcpy(&args[loc], argv[i]);
		loc += strlen(argv[i]) + 1;
	}

	return loc;
}
