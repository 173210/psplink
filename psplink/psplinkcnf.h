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
#include "psplink.h"

#define MAX_BUFFER 1024

struct ConfigFile
{
	int fd;
	char str_buf[MAX_BUFFER];
	char read_buf[MAX_BUFFER];
	int  read_size;
	int  read_pos;
	int  line;
};

int psplinkConfigOpen(const char *filename, struct ConfigFile *cnf);
const char* psplinkConfigReadNext(struct ConfigFile *cnf, const char **name);
void psplinkConfigClose(struct ConfigFile *cnf);
