#ifndef __GDBCOMMON_H__
#define __GDBCOMMON_H__

#define MODULE_NAME "GDBServer"

#define EVENT_HANDLE_EXP  0x1
#define EVENT_CONTINUE    0x2

struct GdbContext
{
	SceUID main_thread;
	PspDebugRegBlock regs;
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
	/* Indicates if we are debugging an ELF or a PRX */
	int elf;
};

extern struct GdbContext g_context;

int _gdbSupportLibReadByte(unsigned char *address, unsigned char *dest);
int _gdbSupportLibWriteByte(char val, unsigned char *dest);
int GdbHandleException (PspDebugRegBlock *regs);
void GdbStubInit(void);
int GdbTrapEntry(PspDebugRegBlock *regs);
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
