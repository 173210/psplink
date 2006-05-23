#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <pspdebug.h>
#include "../psplink_user/psplink_ex.h"

#define VFPU_PRINT_SINGLE 0
#define VFPU_PRINT_COL    1
#define VFPU_PRINT_ROW    2
#define VFPU_PRINT_MATRIX 3
#define VFPU_PRINT_TRANS  4

extern struct PsplinkContext *g_currex;

void exceptionInit(void);
void exceptionPrint(int ex);
void exceptionFpuPrint(int ex);
void exceptionVfpuPrint(int ex, int mode);
u32 *exceptionGetReg(const char *reg);
void exceptionResume(void);
void exceptionPrintFPURegs(float *pFpu, unsigned int fsr, unsigned int fir);
void exceptionPrintCPURegs(u32 *pRegs);
void exceptionList(void);
void exceptionSetCtx(int ex);

#endif
