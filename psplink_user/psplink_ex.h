#ifndef __PSPLINK_EXCEPTION_H__
#define __PSPLINK_EXCEPTION_H__

#include <stdint.h>

/* Define maximum number of thread exception context */
#define PSPLINK_MAX_CONTEXT 16

#define PSPLINK_EXTYPE_NORMAL 0
#define PSPLINK_EXTYPE_DEBUG  1

/** Structure to hold the register data associated with an exception */
typedef struct _PsplinkRegBlock
{
	u32 frame[6];
	/** Array of the 32 GPRs */
	u32 r[32];
	/** The status register */
	u32 status;
	/** lo */
	u32 lo;
	u32 hi;
	u32 badvaddr;
	u32 cause;
	u32 epc;
	float fpr[32];
	u32 fsr;
	u32 fir;
	u32 frame_ptr;
	u32 unused;
	/* Unused on PSP */
	u32 index;
	u32 random;
	u32 entrylo0;
	u32 entrylo1;
	u32 context;
	u32 pagemask;
	u32 wired;
	u32 cop0_7;
	u32 cop0_8;
	u32 cop0_9;
	u32 entryhi;
	u32 cop0_11;
	u32 cop0_12;
	u32 cop0_13;
	u32 cop0_14;
	/* PRId should still be okay */
	u32 prid;
	/* Type of exception (normal or debug) */
	u32 type;
	u32 padding[100];
} PsplinkRegBlock;

/* A thread context during an exception */
struct PsplinkContext
{
	int valid;
	struct PsplinkContext *pNext;
	PsplinkRegBlock regs;
	SceUID thid;
	unsigned int drcntl;
};

int psplinkInitException(void);
int psplinkResumeFromException(struct PsplinkContext *);

#endif
