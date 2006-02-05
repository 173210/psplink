/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * libs.h - Module library code for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#ifndef __LIBS_H__
#define __LIBS_H__

int libsPrintEntries(SceUID uid);
u32 libsFindExportByName(SceUID uid, const char *library, const char *name);
u32 libsFindExportByNid(SceUID uid, const char *library, u32 nid);

#endif
