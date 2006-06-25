/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK USB Remote Joystick Driver
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspkdebug.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <psppower.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usbasync.h>
#include <apihook.h>
#include "remotejoy.h"

PSP_MODULE_INFO("RemoteJoy", PSP_MODULE_KERNEL, 1, 1);

SceCtrlData g_currjoy;
struct AsyncEndpoint g_endp;

#define ASYNC_JOY ASYNC_USER
#define ABS(x) ((x) < 0 ? -x : x)

int map_axis(int real, int new)
{
	int val1, val2;

	val1 = ((int) real) - 127;
	val2 = ((int) new) - 127;

	if(ABS(val1) > ABS(val2))
	{
		return real;
	}
	else
	{
		return new;
	}
}

void add_values(SceCtrlData *pad_data, int count, int neg)
{
	int i;
	int intc;

	intc = pspSdkDisableInterrupts();
	for(i = 0; i < count; i++)
	{
		if(neg)
		{
			pad_data[i].Buttons &= ~g_currjoy.Buttons;
		}
		else
		{
			pad_data[i].Buttons |= g_currjoy.Buttons;
		}

		pad_data[i].Lx = map_axis(pad_data[i].Lx, g_currjoy.Lx);
		pad_data[i].Ly = map_axis(pad_data[i].Ly, g_currjoy.Ly);
	}
	pspSdkEnableInterrupts(intc);
}

int read_buffer_positive(SceCtrlData *pad_data, int count)
{
	int ret;

	ret = sceCtrlReadBufferPositive(pad_data, count);
	if(ret <= 0)
	{
		return ret;
	}

	add_values(pad_data, ret, 0);

	return ret;
}

int peek_buffer_positive(SceCtrlData *pad_data, int count)
{
	int ret;

	ret = sceCtrlPeekBufferPositive(pad_data, count);
	if(ret <= 0)
	{
		return ret;
	}

	add_values(pad_data, ret, 0);

	return ret;
}

int read_buffer_negative(SceCtrlData *pad_data, int count)
{
	int ret;

	ret = sceCtrlReadBufferNegative(pad_data, count);
	if(ret <= 0)
	{
		return ret;
	}

	add_values(pad_data, ret, 1);

	return ret;
}

int peek_buffer_negative(SceCtrlData *pad_data, int count)
{
	int ret;

	ret = sceCtrlPeekBufferNegative(pad_data, count);
	if(ret <= 0)
	{
		return ret;
	}

	add_values(pad_data, ret, 1);

	return ret;
}

int main_thread(SceSize args, void *argp)
{
	SceModule *pMod;
	struct JoyEvent joyevent;
	int intc;

	pMod = sceKernelFindModuleByName("sceController_Service");
	if(pMod == NULL)
	{
		printf("Could not get controller module\n");
		sceKernelTerminateDeleteThread(0);
	}

	if(apiHookByName(pMod->modid, "sceCtrl", "sceCtrlReadBufferPositive", read_buffer_positive) == 0)
	{
		printf("Could not hook controller function\n");
		sceKernelTerminateDeleteThread(0);
	}

	if(apiHookByName(pMod->modid, "sceCtrl", "sceCtrlPeekBufferPositive", peek_buffer_positive) == 0)
	{
		printf("Could not hook controller function\n");
		sceKernelTerminateDeleteThread(0);
	}

	if(apiHookByName(pMod->modid, "sceCtrl", "sceCtrlReadBufferNegative", peek_buffer_negative) == 0)
	{
		printf("Could not hook controller function\n");
		sceKernelTerminateDeleteThread(0);
	}

	if(apiHookByName(pMod->modid, "sceCtrl", "sceCtrlPeekBufferNegative", peek_buffer_negative) == 0)
	{
		printf("Could not hook controller function\n");
		sceKernelTerminateDeleteThread(0);
	}

	pMod = sceKernelFindModuleByName("sceVshBridge_Driver");

	/* Ignore if we dont find vshbridge */
	if(pMod)
	{
		if(apiHookByName(pMod->modid, "sceVshBridge","vshCtrlReadBufferPositive", read_buffer_positive) == 0)
		{
			printf("Could not hook controller function\n");
		}
	}

	if(usbAsyncRegister(ASYNC_JOY, &g_endp) < 0)
	{
		printf("Could not register remotejoy provider\n");
		sceKernelTerminateDeleteThread(0);
	}

	usbWaitForConnect();

	while(1)
	{
		int len;
		len = usbAsyncRead(ASYNC_JOY, (void*) &joyevent, sizeof(joyevent));

		if((len != sizeof(joyevent)) || (joyevent.magic != JOY_MAGIC))
		{
			if(len < 0)
			{
				/* Delay thread, necessary to ensure that the kernel can reboot :) */
				sceKernelDelayThread(250000);
			}
			else
			{
				printf("Invalid read size %d\n", len);
				usbAsyncFlush(ASYNC_JOY);
			}
			continue;
		}

		intc = pspSdkDisableInterrupts();
		switch(joyevent.type)
		{
			case TYPE_BUTTON_UP: g_currjoy.Buttons &= ~joyevent.value;
								 break;
			case TYPE_BUTTON_DOWN: g_currjoy.Buttons |= joyevent.value;
								 break;  
			case TYPE_ANALOG_Y: g_currjoy.Ly = joyevent.value;
								break;
			case TYPE_ANALOG_X: g_currjoy.Lx = joyevent.value;
								break;
			default: break;
		};
		pspSdkEnableInterrupts(intc);
		scePowerTick(0);
	}

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	memset(&g_currjoy, 0, sizeof(g_currjoy));
	g_currjoy.Lx = 0x80;
	g_currjoy.Ly = 0x80;
	/* Create a high priority thread */
	thid = sceKernelCreateThread("RemoteJoy", main_thread, 15, 0x800, 0, NULL);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, args, argp);
	}
	return 0;
}

/* Module stop entry */
int module_stop(SceSize args, void *argp)
{
	return 0;
}
