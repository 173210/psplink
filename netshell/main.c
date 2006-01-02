/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * main.c - Network shell for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Some small parts (c) 2005 PSPPet
 *
 * $Id$
 * $HeadURL$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#define MODULE_NAME "NetShell"
#define WELCOME_MESSAGE "Welcome to PSPLink's NetShell\n"

PSP_MODULE_INFO(MODULE_NAME, 0, 1, 1);
PSP_MAIN_THREAD_NAME("NetShell");

#define printf pspDebugScreenPrintf

int psplinkParseCommand(char *command, int direct_term);
void psplinkPrintPrompt(void);
void psplinkExitShell(void);
void stdoutSetWifiHandler(PspDebugPrintHandler wifiHandler);

int g_currsock = -1;
int g_servsock = -1;

#define SERVER_PORT 23

int wifiPrint(const char *data, int size)
{
	if(g_currsock >= 0)
	{
		sceNetInetSend(g_currsock, data, size, 0);
	}

	return size;
}

int make_socket(uint16_t port)
{
	int sock;
	int ret;
	struct sockaddr_in name;

	sock = sceNetInetSocket(PF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		return -1;
	}

	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = sceNetInetBind(sock, (struct sockaddr *) &name, sizeof(name));
	if(ret < 0)
	{
		return -1;
	}

	return sock;
}

/* Start a simple tcp echo server */
void start_server(const char *szIpAddr)
{
	int ret;
	int sock;
	int new = -1;
	struct sockaddr_in client;
	size_t size;
	int readbytes;
	int  pos = 0;
	char cli[1024];
	char data;

	stdoutSetWifiHandler(wifiPrint);

	/* Create a socket for listening */
	sock = make_socket(SERVER_PORT);
	if(sock < 0)
	{
		printf("Error creating server socket\n");
		return;
	}
	g_servsock = sock;

	ret = sceNetInetListen(sock, 1);
	if(ret < 0)
	{
		printf("Error calling listen\n");
		return;
	}

	printf("Listening for connections ip %s port %d\n", szIpAddr, SERVER_PORT);

	while(1)
	{
		new = sceNetInetAccept(sock, (struct sockaddr *) &client, &size);
		if(new < 0)
		{
			printf("Error in accept %s\n", strerror(errno));
			sceNetInetClose(sock);
			g_servsock = -1;
			return;
		}

		Kprintf("New connection %d from %s:%d\n", new, 
				inet_ntoa(client.sin_addr),
				ntohs(client.sin_port));

		g_currsock = new;

		sceNetInetSend(new, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE), 0);
		psplinkPrintPrompt();

		while(1)
		{
			readbytes = sceNetInetRecv(new, &data, 1, 0);
			if(readbytes <= 0)
			{
				Kprintf("Socket %d closed\n", new);
				sceNetInetClose(new);
				g_currsock = -1;
				break;
			}
			else
			{
				if((data == 10) || (data == 13))
				{
					if(pos > 0)
					{
						cli[pos] = 0;
						pos = 0;
						if(psplinkParseCommand(cli, 0) == 1)
						{
							sceNetInetClose(sock);
							sceNetInetClose(new);
							g_servsock = -1;
							g_currsock = -1;
							psplinkExitShell();
						}
						psplinkPrintPrompt();
					}
				}
				else if(pos < (sizeof(cli) -1))
				{
					cli[pos++] = data;
				}
				else
				{
					/* Do nothing */
				}
			}
		}
	}

	sceNetInetClose(sock);
	g_servsock = -1;
}

/* Connect to an access point */
int connect_to_apctl(int config)
{
	int err;
	int stateLast = -1;

	/* Connect using the first profile */
	err = sceNetApctlConnect(config);
	if (err != 0)
	{
		printf(MODULE_NAME ": sceNetApctlConnect returns %08X\n", err);
		return 0;
	}

	printf(MODULE_NAME ": Connecting...\n");
	while (1)
	{
		int state;
		err = sceNetApctlGetState(&state);
		if (err != 0)
		{
			printf(MODULE_NAME ": sceNetApctlGetState returns $%x\n", err);
			break;
		}
		if (state > stateLast)
		{
			printf("  connection state %d of 4\n", state);
			stateLast = state;
		}
		if (state == 4)
			break;  // connected with static IP

		// wait a little before polling again
		sceKernelDelayThread(50*1000); // 50ms
	}
	printf(MODULE_NAME ": Connected!\n");

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

	pspDebugScreenInit();
	printf("PSPLink NetShell (c) 2k6 TyRaNiD\n");
	do
	{
		if((err = pspSdkInetInit()))
		{
			printf(MODULE_NAME ": Error, could not initialise the network %08X\n", err);
			break;
		}

		if(!sceWlanGetSwitchState())
		{
			printf("Please switch on WLAN on your PSP\n");
			do
			{
				sceKernelDelayThread(1000000);
			}
			while(!sceWlanGetSwitchState());
		}

		if(connect_to_apctl(1))
		{
			// connected, get my IPADDR and run test
			char szMyIPAddr[32];
			if (sceNetApctlGetInfo(8, szMyIPAddr) != 0)
				strcpy(szMyIPAddr, "unknown IP address");

			start_server(szMyIPAddr);
		}
	}
	while(0);

	sceKernelSleepThread();

	return 0;
}

int module_stop(SceSize args, void *argp)
{
	if(g_currsock >= 0)
	{
		sceNetInetClose(g_currsock);
	}
	if(g_servsock >= 0)
	{
		sceNetInetClose(g_servsock);
	}
	(void) pspSdkInetTerm();

	return 0;
}
