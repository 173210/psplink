#ifndef __GDBCOMMON_H__
#define __GDBCOMMON_H__

#define GDB_MODULE_NAME "GDBServer"

#define EVENT_HANDLE_EXP  0x1
#define EVENT_CONTINUE    0x2

#include "../psplink_user/psplink_ex.h"

struct ExceptionMsg
{
	struct ExceptionMsg *link;
	struct PsplinkContext *ctx;
};

struct GdbContext
{
	SceUID main_thread;
	struct PsplinkContext ctx;
	/* Module args */
	char **argv;
	int  argc;
	/* Module UID */
	SceUID uid; 
	/* Module information data */
	SceKernelModuleInfo info;
	/* Indicates whether we have started or not */
	int started;
	/* Thread event flag */
	SceUID evid;
	/* Message Box */
	SceUID mbx;
	/* Indicates if we are debugging an ELF or a PRX */
	int elf;
	/* Indicates if we have been enable to enable hardware debugging */
	int hw;
	unsigned int iaddr; /* HW instruction BP address */
	unsigned int daddr; /* HW data BP address */
	int datatype;
};

#define DATABP_TYPE_READ   1
#define DATABP_TYPE_WRITE  2
#define DATABP_TYPE_ACCESS 3

extern struct GdbContext g_context;

int GdbReadByte(unsigned char *address, unsigned char *dest);
int GdbWriteByte(char val, unsigned char *dest);
int GdbHandleException (struct PsplinkContext *ctx);
void GdbStubInit(void);
int GdbTrapEntry(struct PsplinkContext *ctx);
void GdbMain(void);
int psplinkReferModule(SceUID uid, SceKernelModuleInfo *info);

/* Link specific functions */
int isInit(void);
int putDebugChar(unsigned char ch);
int getDebugChar(unsigned char *ch);
int  writeDebugData(void *data, int len);
void start_server(void);
void stop_server(void);

#endif
