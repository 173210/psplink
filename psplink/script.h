/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * script.h - PSPLINK kernel module shell script code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#ifndef __SCRIPT_H__
#define __SCRIPT_H__

int scriptRun(const char *filename, int argc, char **argv, const char *lastmod, int print);

#endif
