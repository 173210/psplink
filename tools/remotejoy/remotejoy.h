/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * remotejoy.h - PSPLINK PC remote joystick handler
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#ifndef __REMOTE_JOY__
#define __REMOTE_JOY__

#define JOY_MAGIC 0x909ACCEF

#define TYPE_BUTTON_DOWN 1
#define TYPE_BUTTON_UP   2
#define TYPE_ANALOG_Y    3
#define TYPE_ANALOG_X    4

struct JoyEvent
{
	unsigned int magic;
	int type;
	unsigned int value;
} __attribute__((packed));

#endif
