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

static void config_usb(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	if(iVal != 0)
	{
		init_usb();
	}
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

static void config_disopt(struct ConfigContext *ctx, const char *szVal, unsigned int iVal)
{
	disasmSetOpts(szVal, 1);
}

struct psplink_config config_names[] = {
	{ "usb", 1, config_usb },
	{ "baud", 1, config_baud },
	{ "modload", 0, config_modload },
	{ "pluser", 1, config_pluser },
	{ "prompt", 0, config_prompt },
	{ "resetonexit", 1, config_resetonexit },
	{ "wifishell", 1, config_wifishell },
	{ "sioshell", 1, config_sioshell },
	{ "pcterm", 1, config_pcterm },
	{ "wifi", 1, config_wifi },
	{ "path", 0, config_path },
	{ "disopt", 0, config_disopt },
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
	if(ctx->wifishell == 0)
	{
		ctx->sioshell = 1;
	}
}

