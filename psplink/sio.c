/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * sio.c - PSPLINK kernel module sio code
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
#include "psplink.h"
#include "util.h"

static SceUID g_eventflag = -1;
static int g_enablekprintf = 0;

/* Define some important parameters, not really sure on names. Probably doesn't matter */
#define PSP_UART4_FIFO 0xBE500000
#define PSP_UART4_STAT 0xBE500018
#define PSP_UART4_DIV1 0xBE500024
#define PSP_UART4_DIV2 0xBE500028
#define PSP_UART4_CTRL 0xBE50002C
#define PSP_UART_CLK   96000000
#define PSP_UART_TXFULL  0x20
#define PSP_UART_RXEMPTY 0x10

/* Some function prototypes we will need */
int sceHprmEnd(void);
int sceSysregUartIoEnable(int uart);
int sceSyscon_driver_44439604(int power);

void sioPutchar(int ch)
{
	while(_lw(PSP_UART4_STAT) & PSP_UART_TXFULL);
	_sw(ch, PSP_UART4_FIFO);
}

int sioGetchar(void)
{
	if(_lw(PSP_UART4_STAT) & PSP_UART_RXEMPTY)
	{
		return -1;
	}

	return _lw(PSP_UART4_FIFO);
}

/* Put data to SIO converting any line feeds as necessary */
int sioPutText(const char *data, int len)
{
	int i;

	for(i = 0; i < len; i++)
	{
		/* If just line feed add a carriage return */
		if(data[i] == '\n')
		{
			if(((i > 0) && (data[i-1] != '\r')) || (i == 0))
			{
				sioPutchar('\r');
			}
		}

		sioPutchar(data[i]);

		if((i < (len - 1)) && (data[i] == '\r') && (data[i+1] != '\n'))
		{
			sioPutchar('\n');
		}
	}

	return len;
}

void sioSetBaud(int baud)
{
	int div1, div2;

	/* rate set using the rough formula div1 = (PSP_UART_CLK / baud) >> 6 and
	 * div2 = (PSP_UART_CLK / baud) & 0x3F
	 * The uart4 driver actually uses a slightly different formula for div 2 (it
	 * adds 32 before doing the AND, but it doesn't seem to make a difference
	 */
	div1 = PSP_UART_CLK / baud;
	div2 = div1 & 0x3F;
	div1 >>= 6;

	_sw(div1, PSP_UART4_DIV1);
	_sw(div2, PSP_UART4_DIV2);
	_sw(0x60, PSP_UART4_CTRL);
}

static void _sioInit(void)
{
	/* Shut down the remote driver */
	sceHprmEnd();
	/* Enable UART 4 */
	sceSysregUartIoEnable(4);
	/* Enable remote control power */
	sceSyscon_driver_44439604(1);
}

void _EnablePutchar(void)
{
	u32 *pData;

	pData = get_debug_register();
	*pData |= DEBUG_REG_KPRINTF_ENABLE;
}

static void PutCharDebug(unsigned short *data, unsigned int type)
{
	if(((type & 0xFF00) == 0) && (g_enablekprintf))
	{
		if(type == '\n')
		{
			sioPutchar('\r');
		}

		sioPutchar(type);
	}
}

void sioInstallKprintf(void)
{
	_EnablePutchar();
	sceKernelRegisterDebugPutchar(PutCharDebug);
	g_enablekprintf = 1;
}

void sioEnableKprintf(void)
{
	g_enablekprintf = 1;
}

void sioDisableKprintf(void)
{
	g_enablekprintf = 0;
}

static int intr_handler(void *arg)
{
	u32 stat;

	stat = _lw(0xBE500040);
	_sw(stat, 0xBE500044);

	sceKernelDisableIntr(PSP_HPREMOTE_INT);

	sceKernelSetEventFlag(g_eventflag, EVENT_SIO);

	return -1;
}

/* Read a character with a timeout */
int sioReadCharWithTimeout(void)
{
	int ch;
	u32 result;
	u32 timeout;

	timeout = 500000;
	ch = sioGetchar();
	if(ch == -1)
	{
		sceKernelEnableIntr(PSP_HPREMOTE_INT);
		sceKernelWaitEventFlag(g_eventflag, EVENT_SIO, 0x21, &result, &timeout);
		ch = sioGetchar();
	}

	return ch;
}

int sioReadChar(void)
{
	int ch;
	u32 result;

	ch = sioGetchar();
	if(ch == -1)
	{
		sceKernelEnableIntr(PSP_HPREMOTE_INT);
		sceKernelWaitEventFlag(g_eventflag, EVENT_SIO, 0x21, &result, NULL);

		ch = sioGetchar();
	}

	return ch;
}

void sioInit(int baud, int kponly)
{
	_sioInit();
	if(!kponly)
	{
		g_eventflag = sceKernelCreateEventFlag("SioShellEvent", 0, 0, 0);
		sceKernelRegisterIntrHandler(PSP_HPREMOTE_INT, 1, intr_handler, NULL, NULL);
		sceKernelEnableIntr(PSP_HPREMOTE_INT);
		/* Delay thread for a but */
		sceKernelDelayThread(2000000);
	}
	sioSetBaud(baud);
	sioInstallKprintf();
}
