/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * sio.h - PSPLINK kernel module sio code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#ifndef __SIO_H__
#define __SIO_H__

void sioInit(void);
int sioReadChar(void);
int sioReadCharWithTimeout(void);

#endif
