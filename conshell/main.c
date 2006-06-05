/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * main.c - Console shell for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2006 Rasmus B
 *
 * $Id$
 * $HeadURL$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MODULE_NAME "ConShell"

PSP_MODULE_INFO(MODULE_NAME, 0, 1, 1);
PSP_MAIN_THREAD_NAME("ConShell");

int psplinkParseCommand(char *command);
void psplinkPrintPrompt(void);
void psplinkExitShell(void);
int psplinkConsolePermit(void);
void ttySetConsHandler(PspDebugPrintHandler consHandler);

int consPrint(const char *data, int size) {
	char buf[512];
	int i;

	size = (size < 512)?size:511;
	memcpy(buf, data, size);
	buf[size] = '\0';
	for (i = 0; i < size; i++) {
		if (buf[i] == 13) {
			buf[i] = ' ';
		}
	}
	pspDebugScreenPrintf(buf);
	return size;
}

void run_conshell() {
	SceCtrlData pad;
	unsigned int oldbuttons = 0;
	char* cli;

	ttySetConsHandler(consPrint);
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	psplinkPrintPrompt();

	while(1) {
		unsigned int pressed;
		char sendcommand = 0;

		sceCtrlPeekBufferPositive(&pad, 1);
		pressed = pad.Buttons & (~oldbuttons);
		oldbuttons = pad.Buttons;

		if (pressed & PSP_CTRL_LTRIGGER) {
			cli = "exit";
			printf("%s\n", cli);
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_RTRIGGER) {
			cli = "reset";
			printf("%s\n", cli);
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_CROSS) {
			cli = "custom 0";
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_SQUARE) {
			cli = "custom 1";
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_TRIANGLE) {
			cli = "custom 2";
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_CIRCLE) {
			cli = "custom 3";
			sendcommand = 1;		
		} else if (pressed & PSP_CTRL_SELECT) {
			cli = "custom 4";
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_START) {
			cli = "custom 5";
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_DOWN) {
			cli = "custom 6";
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_LEFT) {
			cli = "custom 7";
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_UP) {
			cli = "custom 8";
			sendcommand = 1;
		} else if (pressed & PSP_CTRL_RIGHT) {
			cli = "custom 9";
			sendcommand = 1;
		}

		if ((sendcommand) && psplinkConsolePermit()) {
			if (psplinkParseCommand(cli) == 1) {
				psplinkExitShell();
			}
		}
		sceKernelDelayThread(100 * 1000);
	}
}

/* Simple thread */
int main(int argc, char **argv) {
	pspDebugScreenPrintf("PSPLink ConShell (c) 2k6 TyRaNiD, rasmusb\n");
	run_conshell();
	sceKernelSleepThread();
	return 0;
}

int module_stop(SceSize args, void *argp) {
	return 0;
}
