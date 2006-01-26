#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <pspdebug.h>

struct ExceptionContext
{
	PspDebugRegBlock regs;
	int exception;
	SceUID thid;
	char threadname[32];
	SceUID modid;
	char modulename[32];
};

extern struct ExceptionContext g_exception;

void exceptionInit(void);
void exceptionPrint(void);
u32 *exceptionGetReg(const char *reg);
void exceptionResume(void);

#endif
