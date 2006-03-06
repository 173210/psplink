#ifndef __NETGDB_H__
#define __NETGDB_H__

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
int putDebugChar(unsigned char ch);
int getDebugChar(unsigned char *ch);
int  writeDebugData(void *data, int len);
int GdbHandleException (PspDebugRegBlock *regs);
void GdbStubInit(void);

int psplinkReferModule(SceUID uid, SceKernelModuleInfo *info);

#endif
