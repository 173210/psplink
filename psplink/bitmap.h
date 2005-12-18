/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * bitmap.h - PSPLINK kernel module bitmap code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#ifndef __BITMAP_H__
#define __BITMAP_H__

int bitmapWrite(void *frame_addr, int format, const char *file);

#endif
