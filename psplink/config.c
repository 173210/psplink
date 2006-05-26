/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * config.c - PSPLINK kernel module configuration loader.
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
#include <psputilsforkernel.h>
#include <pspsysmem_kernel.h>
#include "memoryUID.h"
#include "psplink.h"
#include "psplinkcnf.h"
#include "parse_args.h"
#include "util.h"
#include "sio.h"
#include "shell.h"
#include "disasm.h"
#include "config.h"

struct psplink_config
{
	const char *name;
	int   isnum;
	void (*handler)(struct ConfigContext *ctx, const char *szVal, unsigned int iVal);
};

static void config_usbmass(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->usbmass = iVal;
}

static void config_usbhost(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->usbhost = iVal;
}

static void config_baud(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	int valid = 0;

	switch(iVal)
	{
		case 4800:
		case 9600:
		case 19200:
		case 38400:
		case 57600:
		case 115200: valid = 1;
					 break;
		default: break;
	};

	if(valid)
	{
		printf("Setting baud to %d\n", iVal);
		ctx->baudrate = iVal;
	}
	else
	{
		printf("Invalid baud rate %d\n", iVal);
		/* Set a default */
		ctx->baudrate = DEFAULT_BAUDRATE;

	}
}

static void config_modload(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	(void) load_start_module(szVal, 0, NULL);
}

static void config_pluser(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->enableuser = iVal;
}

static void config_prompt(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	strncpy(ctx->cliprompt, szVal, sizeof(ctx->cliprompt)-1);
	ctx->cliprompt[sizeof(ctx->cliprompt)-1] = 0;
}

static void config_path(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	strncpy(ctx->path, szVal, sizeof(ctx->path)-1);
	ctx->path[sizeof(ctx->path)-1] = 0;
}

static void config_resetonexit(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->resetonexit = iVal;
}

static void config_wifishell(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->wifishell = iVal;
	if((iVal) && (ctx->wifi == 0))
	{
		/* Set default wifi access point */
		ctx->wifi = 1;
	}
}

#ifndef USB_ONLY

static void config_conshell(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	ctx->conshell = iVal;
}

static void config_conscrosscmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->conscrosscmd)-1;
	char* dest = ctx->conscrosscmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_conssquarecmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->conssquarecmd)-1;
	char* dest = ctx->conssquarecmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_constrianglecmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->constrianglecmd)-1;
	char* dest = ctx->constrianglecmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_conscirclecmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->conscirclecmd)-1;
	char* dest = ctx->conscirclecmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_consselectcmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->consselectcmd)-1;
	char* dest = ctx->consselectcmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_consstartcmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->consstartcmd)-1;
	char* dest = ctx->consstartcmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_consdowncmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->consdowncmd)-1;
	char* dest = ctx->consdowncmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_consleftcmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->consleftcmd)-1;
	char* dest = ctx->consleftcmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_consupcmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->consupcmd)-1;
	char* dest = ctx->consupcmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_consrightcmd(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	const int len = sizeof(ctx->consrightcmd)-1;
	char* dest = ctx->consrightcmd;
	strncpy(dest, szVal, len);
	dest[len] = 0;
}

static void config_consinterfere(struct ConfigContext *ctx, const char* szVal, unsigned int iVal)
{
	ctx->consinterfere = iVal;
}

#endif

static void config_sioshell(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->sioshell = iVal;
}

static void config_pcterm(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->pcterm = iVal;
}

static void config_wifi(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->wifi = iVal;
}

static void config_usbshell(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	if(iVal)
	{
		ctx->usbhost = 1;
		ctx->usbshell = 1;
	}
}

static void config_usbgdb(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->usbgdb = iVal;
}

static void config_disopt(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	disasmSetOpts(szVal, 1);
}

static void config_kprintf(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	ctx->kprintf = iVal;
}

struct psplink_config config_names[] = {
	{ "usbmass", 1, config_usbmass },
	{ "usbhost", 1, config_usbhost },
	{ "baud", 1, config_baud },
	{ "modload", 0, config_modload },
	{ "pluser", 1, config_pluser },
	{ "prompt", 0, config_prompt },
	{ "resetonexit", 1, config_resetonexit },
	{ "wifishell", 1, config_wifishell },
	{ "sioshell", 1, config_sioshell },
	{ "pcterm", 1, config_pcterm },
#ifndef USB_ONLY
	{ "conshell", 1, config_conshell },
	{ "consinterfere", 1, config_consinterfere },
	{ "conscrosscmd", 0, config_conscrosscmd },
	{ "conssquarecmd", 0, config_conssquarecmd },
	{ "constrianglecmd", 0, config_constrianglecmd },
	{ "conscirclecmd", 0, config_conscirclecmd },
	{ "consselectcmd", 0, config_consselectcmd },
	{ "consstartcmd", 0, config_consstartcmd },
	{ "consdowncmd", 0, config_consdowncmd },
	{ "consleftcmd", 0, config_consleftcmd },
	{ "consupcmd", 0, config_consupcmd },
	{ "consrightcmd", 0, config_consrightcmd },
#endif
	{ "wifi", 1, config_wifi },
	{ "path", 0, config_path },
	{ "disopt", 0, config_disopt },
	{ "usbshell", 1, config_usbshell },
	{ "usbgdb", 1, config_usbgdb },
	{ "kprintf", 1, config_kprintf },
	{ NULL, 0, NULL }
};

void configLoad(const char *bootpath, struct ConfigContext *ctx)
{
	char cnf_path[256];
	struct ConfigFile cnf;

	memset(ctx, 0, sizeof(*ctx));
	strcpy(cnf_path, bootpath);
	strcat(cnf_path, "psplink.ini");
	printf("Config Path %s\n", cnf_path);
	if(psplinkConfigOpen(cnf_path, &cnf))
	{
		const char *name;
		const char *val;

		while((val = psplinkConfigReadNext(&cnf, &name)))
		{
			int config;

			config = 0;
			while(config_names[config].name)
			{
				if(strcmp(config_names[config].name, name) == 0)
				{
					unsigned int iVal = 0;
					if(config_names[config].isnum)
					{
						char *endp;

						iVal = strtoul(val, &endp, 10);
						if(*endp != 0)
						{
							printf("Error, line %d value should be a number\n", cnf.line); 
							break;
						}
					}

					config_names[config].handler(ctx, val, iVal);
				}
				config++;
			}

			/* Ignore anything we don't care about */
		}

		psplinkConfigClose(&cnf);
	}

	/* Always enable at least the sio shell */
	if((ctx->usbshell == 0) && (ctx->wifishell == 0))
	{
		ctx->sioshell = 1;
	}
}

void configPrint(const char *bootpath)
{
	char cnf_path[256];
	struct ConfigFile cnf;

	strcpy(cnf_path, bootpath);
	strcat(cnf_path, "psplink.ini");
	printf("Config Path %s\n", cnf_path);
	if(psplinkConfigOpen(cnf_path, &cnf))
	{
		const char *name;
		const char *val;

		while((val = psplinkConfigReadNext(&cnf, &name)))
		{
			printf("%s=%s\n", name, val);
		}

		psplinkConfigClose(&cnf);
	}
}

void configChange(const char *bootpath, const char *newname, const char *newval, int mode)
{
	char cnf_path[256];
	char new_path[256];
	int found = 0;
	struct ConfigFile cnf;
	int fd = -1;

	if((mode != CONFIG_MODE_ADD) && (mode != CONFIG_MODE_DEL))
	{
		return;
	}

	strcpy(cnf_path, bootpath);
	strcat(cnf_path, "psplink.ini");
	printf("Config Path %s\n", cnf_path);

	strcpy(new_path, bootpath);
	strcat(new_path, "psplink.ini.tmp");
	fd = sceIoOpen(new_path, PSP_O_WRONLY | PSP_O_TRUNC | PSP_O_CREAT, 0777);
	if(fd >= 0)
	{
		if(psplinkConfigOpen(cnf_path, &cnf))
		{
			const char *name;
			const char *val;

			while((val = psplinkConfigReadNext(&cnf, &name)))
			{
				if(strcmp(name, newname) == 0)
				{
					if(mode == CONFIG_MODE_ADD)
					{
						fdprintf(fd, "%s=\"%s\"\n", newname, newval);
						found = 1;
					}
				}
				else
				{
					fdprintf(fd, "%s=\"%s\"\n", name, val);
				}
			}

			if((mode == CONFIG_MODE_ADD) && (!found))
			{
				fdprintf(fd, "%s=\"%s\"\n", newname, newval);
			}

			sceIoClose(fd);
			fd = -1;
			psplinkConfigClose(&cnf);

			if(sceIoRemove(cnf_path) < 0)
			{
				printf("Error deleting original configuration\n");
			}
			else
			{
				if(sceIoRename(new_path, cnf_path) < 0)
				{
					printf("Error renaming configuration\n");
				}
			}
		}
		else
		{
			printf("Couldn't open temporary config file %s\n", new_path);
		}

		if(fd >= 0)
		{
			sceIoClose(fd);
			sceIoRemove(new_path);
		}
	}
}
