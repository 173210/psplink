/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * netgdb.c - Main code for network GDB Server
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $Id: main.c 1789 2006-02-05 18:17:47Z tyranid $
 * $HeadURL: svn://tyranid@svn.pspdev.org/psp/trunk/psplink/netgdb/main.c $
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
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <modnet.h>
#include "gdb-common.h"
#include "../psplink_user/psplink_user.h"

#define SERVER_PORT 6999

static int g_servsock = -1;
static int g_sock = -1;

int isInit(void)
{
	if(modNetIsInit() >= 0)
	{
		return 1;
	}

	return 0;
}

int putDebugChar(unsigned char ch)
{
	if(g_sock >= 0)
	{
		return sceNetInetSend(g_sock, &ch, 1, 0);
	}

	return -1;
}

int getDebugChar(unsigned char *ch)
{
	*ch = 0;
	if(g_sock >= 0)
	{
		return sceNetInetRecv(g_sock, ch, 1, 0);
	}

	return -1;
}

int writeDebugData(void *data, int len)
{
	if(g_sock >= 0)
	{
		return sceNetInetSend(g_sock, data, len, 0);
	}

	return -1;
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
void start_server(void)
{
	const char *ip;
	int ret;
	struct sockaddr_in client;
	size_t size;
	int flag = 1;

	ip = modNetGetIpAddress();

	/* Create a socket for listening */
	g_servsock = make_socket(SERVER_PORT);
	if(g_servsock < 0)
	{
		printf(MODULE_NAME ": Error creating server socket\n");
		return;
	}

	ret = sceNetInetListen(g_servsock, 1);
	if(ret < 0)
	{
		printf(MODULE_NAME ": Error calling listen\n");
		return;
	}

	printf(MODULE_NAME ": Listening for connections ip %s port %d\n", ip, SERVER_PORT);

	while(1)
	{
		g_sock = sceNetInetAccept(g_servsock, (struct sockaddr *) &client, &size);
		if(g_sock < 0)
		{
			printf(MODULE_NAME ": Error in accept\n");
			return;
		}

		sceNetInetSetsockopt(g_sock, SOL_TCP, TCP_NODELAY, &flag, sizeof(int));

		printf(MODULE_NAME ": New connection %d from %s:%d\n", g_sock, 
				inet_ntoa(client.sin_addr), ntohs(client.sin_port));

		GdbMain();
	}
}

void stop_server(void)
{
	if(g_sock >= 0)
	{
		sceNetInetClose(g_sock);
	}

	if(g_servsock >= 0)
	{
		sceNetInetClose(g_servsock);
	}

}
