/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - Main code for MODNET user module.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Some parts (c) 2005 PSPPet
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
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <pspwlan.h>
#include <netinet/in.h>

#define MODULE_NAME "MODNET"

PSP_MODULE_INFO(MODULE_NAME, 0, 1, 1);
PSP_MAIN_THREAD_NAME("ModNet");

#define MAX_ARGS 16

#define EVENT_INIT 1

#define SANITY_COUNT 16
#define SANITY_POLL  (50*1000)

SceUID g_modevent = -1;
char   g_ipaddr[20] = "";

void psplinkUserRegisterGdbHandler(int i);

int modNetIsInit(void)
{
	int sanity = 0;
	unsigned int result;

	/* Wait for the event flag to be created */
	for(sanity = 0; sanity < SANITY_COUNT; sanity++)
	{
		if(g_modevent < 0)
		{
			sceKernelDelayThread(SANITY_POLL);
		}
		else
		{
			break;
		}
	}

	if(sanity == SANITY_COUNT)
	{
		return -1;
	}

	return sceKernelWaitEventFlag(g_modevent, EVENT_INIT, 0x1, &result, NULL);
}

const char *modNetGetIpAddress(void)
{
	return g_ipaddr;
}

int connect_to_ap(int config)
{
	int err;
	int stateLast = -1;

	err = sceNetApctlConnect(config);
	if (err != 0)
	{
		pspDebugScreenPrintf(MODULE_NAME ": sceNetApctlConnect returns %08X\n", err);
		return 0;
	}

	// Report status while waiting for connection to access point
	pspDebugScreenPrintf(MODULE_NAME ": Connecting...\n");
	while (1)
	{
		int state;
		err = sceNetApctlGetState(&state);
		if (err != 0)
		{
			pspDebugScreenPrintf(MODULE_NAME ": sceNetApctlGetState returns $%x\n", err);
			break;
		}
		if (state > stateLast)
		{
			pspDebugScreenPrintf("  connection state %d of 4\n", state);
			stateLast = state;
		}
		if (state == 4)
			break;  // connected with static IP

		// wait a little before polling again
		sceKernelDelayThread(50*1000); // 50ms
	}
	pspDebugScreenPrintf(MODULE_NAME ": Connected!\n");

	if(err != 0)
	{
		return 0;
	}

	return 1;
}

/* Simple thread */
int main(int argc, char **argv)
{
	int err;
	int ap = 1;

	if(argc > 1)
	{
		ap = atoi(argv[1]);
	}

	do
	{
		g_modevent = sceKernelCreateEventFlag("ModNetEvent", 0x200, 0, 0);
		if(g_modevent < 0)
		{
			pspDebugScreenPrintf(MODULE_NAME ": Error, could not create eventflag\n");
			break;
		}

		if((err = pspSdkInetInit()))
		{
			pspDebugScreenPrintf(MODULE_NAME ": Error, could not initialise the network %08X\n", err);
			break;
		}
		
		if(!sceWlanGetSwitchState())
		{
			pspDebugScreenPrintf("Please switch on WLAN on your PSP\n");
			do
			{
				sceKernelDelayThread(1000000);
			}
			while(!sceWlanGetSwitchState());
		}

		if(!connect_to_ap(ap))
		{
			pspDebugScreenPrintf(MODULE_NAME ": Error, could not connect to access point\n");
			break;
		}

		if (sceNetApctlGetInfo(8, g_ipaddr) != 0)
		{
			strcpy(g_ipaddr, "unknown IP address");
		}

		err = sceKernelSetEventFlag(g_modevent, EVENT_INIT);
		if(err < 0)
		{
			pspDebugScreenPrintf(MODULE_NAME ": Error, could not set eventflag %08X\n", err);
			break;
		}
	}
	while(0);

	sceKernelSleepThread();

	return 0;
}

int module_stop(SceSize args, void *argp)
{
	(void) pspSdkInetTerm();

	return 0;
}
