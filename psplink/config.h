/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * config.h - PSPLINK kernel module configuration loader.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 * $HeadURL$
 * $Id$
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CONFIG_MODE_ADD 1
#define CONFIG_MODE_DEL 2

struct ConfigContext
{
	/* Indicates whether to enable the psplink user module */
	int  enableuser;
	char cliprompt[128];
	char path[128];
	int  resetonexit;
	int  wifi;
	int  wifishell;
	int  sioshell;
	int  conshell;
	int  consinterfere;
	char conscrosscmd[64];    /* custom 0 */
	char conssquarecmd[64];   /* custom 1 */
	char constrianglecmd[64]; /* custom 2 */
	char conscirclecmd[64];   /* custom 3 */
	char consselectcmd[64];   /* custom 4 */
	char consstartcmd[64];    /* custom 5 */
	char consdowncmd[64];     /* custom 6 */
	char consleftcmd[64];     /* custom 7 */
	char consupcmd[64];	  /* custom 8 */
	char consrightcmd[64];    /* custom 9 */
	int  pcterm;
	int  baudrate;
	int  usbmass;
	int  usbhost;
	int  usbshell;
	int  usbgdb;
	int  kprintf;
};

void configLoad(const char *bootpath, struct ConfigContext *ctx);
void configPrint(const char *bootpath);
void configChange(const char *bootpath, const char *name, const char *val, int mode);

#endif
