#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <pspdebug.h>
#include "../psplink_user/psplink_ex.h"

extern struct PsplinkContext *g_currex;

void exceptionInit(void);
void exceptionPrint(int ex);
void exceptionFpuPrint(int ex);
u32 *exceptionGetReg(const char *reg);
void exceptionResume(void);
void exceptionPrintFPURegs(float *pFpu, unsigned int fsr, unsigned int fir);
void exceptionPrintCPURegs(u32 *pRegs);
void exceptionList(void);
void exceptionSetCtx(int ex);

#endif
