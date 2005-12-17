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

struct ConfigContext
{
	/* Indicates whether to enable the psplink user module */
	int  enableuser;
};

void configLoad(const char *bootpath, struct ConfigContext *ctx);

#endif
