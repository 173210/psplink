/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psplink_user.h -Header for PSPLINK user module.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#ifndef __PSPLINK_USER_H__
#define __PSPLINK_USER_H__

#include "psplink_ex.h"

typedef int (*GdbHandler)(struct PsplinkContext *ctx);

void psplinkUserRegisterGdbHandler(GdbHandler gdbhandler);

#endif
