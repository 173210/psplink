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
int load_start_module(const char *name, int argc, char **argv);
int load_start_module_debug(const char *name);
void map_firmwarerev(void);
int init_usb(void);
int stop_usb(void);
void save_execargs(int argc, char **argv);

extern int (*g_QueryModuleInfo)(SceUID modid, SceKernelModuleInfo *info);
extern int (*g_GetModuleIdList)(SceUID *readbuf, int readbufsize, int *idcount);
