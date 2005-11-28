/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * util.h - header file for util.c
 *
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#define TYPE_FILE	1
#define TYPE_DIR	2
#define TYPE_ETHER	3

char upcase(char ch);
int is_aspace(int ch);
int build_bootargs(char *args, const char *bootfile, const char *execfile, int argc, char **argv);
int build_args(char *args, const char *execfile, int argc, char **argv);
int handlepath(char *currentdir, char *relative, char *path, int type, int validate);
