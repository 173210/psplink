/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * gdb-stub.c - GDB stub for psplink
 *
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://tyranid@svn.pspdev.org/psp/trunk/psplink/netgdb/gdb-stub.c $
 * $Id: gdb-stub.c 1789 2006-02-05 18:17:47Z tyranid $
 */
/* Note: there is the odd small bit which comes from the gdb stubs/linux mips stub */
/* As far as I am aware they didn't have an explicit license on them so... */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <psputilsforkernel.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../psplink/debug.h"
#include "gdb-common.h"

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define DEBUG_PRINTF(fmt, ...)
#endif

/*
 * breakpoint and test functions
 */

extern int sceKernelSuspendIntr(void);
extern void sceKernelResumeIntr(int intr);

#define MAX_BUF 2048

void _GdbExceptionHandler(void);
static int initialised = 0;
static char input[MAX_BUF];
static char output[MAX_BUF];
static const char hexchars[]="0123456789abcdef";
static int attached = 0;

#define SW_BREAK_INST	0x0000000d

/* Define a software breakpoint structure */
struct sw_breakpoint
{
	unsigned int addr;
	unsigned int oldinst;
	unsigned int active;
};

static struct sw_breakpoint g_stepbp[2];

void set_swbp(u32 addr)
{
	memset(g_stepbp, 0, sizeof(struct sw_breakpoint) * 2);
	g_stepbp[0].addr = addr;
	g_stepbp[0].oldinst = _lw(addr);
	g_stepbp[0].active = 1;
	_sw(SW_BREAK_INST, addr);
}

/*
 * send the packet in buffer.
 */
static int putpacket(unsigned char *buffer)
{
	static unsigned char outputbuffer[4096];
	unsigned char checksum;
	int count;
	unsigned char ch;
	int i = 0;

	/*
	 * $<packet info>#<checksum>.
	 */

	do {
		outputbuffer[i++] = '$';
		checksum = 0;
		count = 0;

		while ((ch = buffer[count]) != 0) {
			checksum += ch;
			count += 1;
			outputbuffer[i++] = ch;
		}

		outputbuffer[i++] = '#';

		outputbuffer[i++] = hexchars[(checksum >> 4) & 0xf];

		outputbuffer[i++] = hexchars[checksum & 0xf];

		DEBUG_PRINTF("Writing %.*s\n", i, outputbuffer);
		writeDebugData(outputbuffer, i);

		DEBUG_PRINTF("calculated checksum = %02X\n", checksum);

		if(getDebugChar(&ch) <= 0)
		{
			return 0;
		}
	}
	while ((ch & 0x7f) != '+');

	return 1;
}

/*
 * Convert ch from a hex digit to an int
 */
static int hex(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

/*
 * scan for the sequence $<data>#<checksum>
 */
static int getpacket(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	unsigned char ch;

	do {
		/*
		 * wait around for the start character,
		 * ignore all other characters
		 */
		ch = 0;
		while((ch & 0x7F) != '$')
		{
			if(getDebugChar(&ch) <= 0)
			{
				return 0;
			}
		}

		checksum = 0;
		xmitcsum = -1;
		count = 0;

		/*
		 * now, read until a # or end of buffer is found
		 */
		while (count < MAX_BUF) {
			if(getDebugChar(&ch) <= 0)
			{
				return 0;
			}

			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}

		if (count >= MAX_BUF)
			continue;

		buffer[count] = 0;

		if (ch == '#') {
			if(getDebugChar(&ch) <= 0)
			{
				return 0;
			}
			xmitcsum = hex(ch & 0x7f) << 4;
			if(getDebugChar(&ch) <= 0)
			{
				return 0;
			}
			xmitcsum |= hex(ch & 0x7f);

			if (checksum != xmitcsum)
				putDebugChar('-');	/* failed checksum */
			else {
				putDebugChar('+'); /* successful transfer */

				/*
				 * if a sequence char is present,
				 * reply the sequence ID
				 */
				if (buffer[2] == ':') {
					putDebugChar(buffer[0]);
					putDebugChar(buffer[1]);

					/*
					 * remove sequence chars from buffer
					 */
					count = strlen(buffer);
					for (i=3; i <= count; i++)
						buffer[i-3] = buffer[i];
				}
			}
		}
	}
	while (checksum != xmitcsum);

	return 1;
}

static struct hard_trap_info {
	unsigned char tt;		/* Trap type code for MIPS R3xxx and R4xxx */
	unsigned char signo;		/* Signal that we map this trap into */
} hard_trap_info[] = {
	{ 6, SIGBUS },			/* instruction bus error */
	{ 7, SIGBUS },			/* data bus error */
	{ 9, SIGTRAP },			/* break */
	{ 10, SIGILL },			/* reserved instruction */
	{ 12, SIGFPE },			/* overflow */
	{ 13, SIGTRAP },		/* trap */
	{ 14, SIGSEGV },		/* virtual instruction cache coherency */
	{ 15, SIGFPE },			/* floating point exception */
	{ 23, SIGSEGV },		/* watch */
	{ 31, SIGSEGV },		/* virtual data cache coherency */
	{ 0, 0}				/* Must be last */
};

static int computeSignal(int tt)
{
	struct hard_trap_info *ht;

	for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
		if (ht->tt == tt)
			return ht->signo;

	return SIGHUP;		/* default for things we don't know about */
}

void GdbStubInit(void)
{
	/* Read out any pending data */
	memset(g_stepbp, 0, 2 * sizeof(struct sw_breakpoint));

	initialised = 1;
	attached = 1;
}

static char *mem2hex(unsigned char *mem, char *buf, int count)
{
	unsigned char ch;

	while (count-- > 0) {
		if (GdbReadByte(mem++, &ch) == 0)
		{
			return NULL;
		}

		*buf++ = hexchars[(ch >> 4) & 0xf];
		*buf++ = hexchars[ch & 0xf];
	}

	*buf = 0;

	return buf;
}

static char *hex2mem(char *buf, char *mem, int count, int binary)
{
	int i;
	unsigned char ch;

	for (i=0; i<count; i++)
	{
		if (binary) {
			ch = *buf++;
			if (ch == 0x7d)
				ch = 0x20 ^ *buf++;
		}
		else {
			ch = hex(*buf++) << 4;
			ch |= hex(*buf++);
		}
		if (GdbWriteByte(ch, (unsigned char *) mem++) == 0)
			return 0;
	}

	return mem;
}

static int hexToInt(char **ptr, unsigned int *intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue < 0)
			break;

		*intValue = (*intValue << 4) | hexValue;
		numChars ++;

		(*ptr)++;
	}

	return numChars;
}

static char *strtohex(char *ptr, const char *str)
{
	while(*str)
	{
		*ptr++ = hexchars[*str >> 4];
		*ptr++ = hexchars[*str & 0xf];
		str++;
	}
	*ptr = 0;

	return ptr;
}

/* Define some opcode stuff for the stepping function */
#define BEQ_OPCODE		0x4
#define BEQL_OPCODE		0x14
#define BGTZ_OPCODE 	0x7
#define BGTZL_OPCODE	0x17
#define BLEZ_OPCODE		0x6
#define BLEZL_OPCODE	0x16
#define BNE_OPCODE		0x5
#define BNEL_OPCODE		0x15

/* Reg Imm */
#define REGIMM_OPCODE 	0x1
#define BGEZ_OPCODE		0x1
#define BGEZAL_OPCODE	0x11
#define BGEZALL_OPCODE	0x13
#define BGEZL_OPCODE	0x3
#define BLTZ_OPCODE		0
#define BLTZAL_OPCODE	0x10
#define BLTZALL_OPCODE	0x12
#define BLTZL_OPCODE	0x2

#define J_OPCODE		0x2
#define JAL_OPCODE		0x3

/* Special opcode */
#define SPECIAL_OPCODE	0
#define JALR_OPCODE		0x9
#define JR_OPCODE		0x8
#define SYSCALL_OPCODE  0xc

/* Cop Branches (all the same) */
#define COP0_OPCODE		0x10
#define COP1_OPCODE		0x11
#define COP2_OPCODE		0x12
#define BCXF_OPCODE		0x100
#define BCXFL_OPCODE	0x102
#define BCXT_OPCODE		0x101
#define BCXTL_OPCODE	0x103

/* Generic step command , if skip then will try to skip over jals */
static void step_generic(struct PsplinkContext *ctx, int skip)
{
	u32 opcode;
	u32 epc;
	u32 targetpc;
	int branch = 0;
	int cond   = 0;
	int link   = 0;

	epc = ctx->regs.epc;
	targetpc = epc + 4;

	opcode = _lw(epc);

	switch(opcode >> 26)
	{
		case BEQ_OPCODE:
		case BEQL_OPCODE:
		case BGTZ_OPCODE:
		case BGTZL_OPCODE:
		case BLEZ_OPCODE:
		case BLEZL_OPCODE: 
		case BNE_OPCODE:
		case BNEL_OPCODE:
						    {
							   short ofs;

							   ofs = (short) (opcode & 0xffff);
							   cond = 1;
							   branch = 1;
							   targetpc += ofs * 4;
						   }
						   break;
		case REGIMM_OPCODE: {
								switch((opcode >> 16) & 0x1f)
								{
									case BGEZ_OPCODE:
									case BGEZAL_OPCODE:
									case BGEZALL_OPCODE:	
									case BGEZL_OPCODE:
									case BLTZ_OPCODE:
									case BLTZAL_OPCODE:
									case BLTZALL_OPCODE:
									case BLTZL_OPCODE: {
														   short ofs;

														   ofs = (short) (opcode & 0xffff);
														   cond = 1;
														   branch = 1;
														   targetpc += ofs * 4;
													   }
													   break;
								}
						    }
							break;
		case JAL_OPCODE:	link = 1;
		case J_OPCODE: {
							 u32 ofs;
							 
							 ofs = opcode & 0x3ffffff;
							 targetpc = (ofs << 2) | (targetpc & 0xf0000000);
							 branch = 1;
							 cond = 0;
						 }
						 break;
		case SPECIAL_OPCODE:
						 {
							 switch(opcode & 0x3f)
							 {
								 case JALR_OPCODE: link = 1;
								 case JR_OPCODE:
												 {
													 u32 rs;

													 rs = (opcode >> 21) & 0x1f;
													 targetpc = ctx->regs.r[rs];
													 branch = 1;
													 cond = 0;
												 }
												 break;
								 case SYSCALL_OPCODE:
												 targetpc = ctx->regs.r[31];
												 break;
							 };
						 }
						 break;
		case COP0_OPCODE:
		case COP1_OPCODE:
		case COP2_OPCODE:
						 {
							 switch((opcode >> 16) & 0x3ff)
							 {
								 case BCXF_OPCODE:
								 case BCXFL_OPCODE:
								 case BCXT_OPCODE:
								 case BCXTL_OPCODE:
									 				{
														short ofs;

														ofs = (short) (opcode & 0xffff);
														cond = 1;
														branch = 1;
														targetpc += ofs * 4;
													}
													break;
							 };
						 }
						 break;
	};

	if(link && skip)
	{
		g_stepbp[1].addr = epc + 8;
		g_stepbp[1].oldinst = _lw(epc + 8);
		g_stepbp[1].active = 1;
		_sw(SW_BREAK_INST, epc + 8);
	}
	else if(branch)
	{
		g_stepbp[0].addr = targetpc;
		g_stepbp[0].oldinst = _lw(targetpc);
		g_stepbp[0].active = 1;
		_sw(SW_BREAK_INST, targetpc);
			
		if((cond) && (targetpc != (epc + 8)))
		{
			g_stepbp[1].addr = epc + 8;
			g_stepbp[1].oldinst = _lw(epc + 8);
			g_stepbp[1].active = 1;
			_sw(SW_BREAK_INST, epc + 8);
		}

	}
	else
	{
		g_stepbp[0].addr = targetpc;
		g_stepbp[0].active = 1;
		g_stepbp[0].oldinst = _lw(targetpc);
		_sw(SW_BREAK_INST, targetpc);
	}
}

void build_trap_cmd(int sigval, struct PsplinkContext *ctx)
{
	char *ptr;
	/*
	 * reply to host that an exception has occurred
	 */
	ptr = output;
	*ptr++ = 'T';
	*ptr++ = hexchars[(sigval >> 4) & 0xf];
	*ptr++ = hexchars[sigval & 0xf];

	/*
	 * Send Error PC
	 */
	*ptr++ = hexchars[37 >> 4];
	*ptr++ = hexchars[37 & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((unsigned char *) &ctx->regs.epc, ptr, sizeof(u32));
	*ptr++ = ';';

	/*
	 * Send frame pointer
	 */
	*ptr++ = hexchars[30 >> 4];
	*ptr++ = hexchars[30 & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((unsigned char *)&ctx->regs.r[30], ptr, sizeof(u32));
	*ptr++ = ';';

	/*
	 * Send stack pointer
	 */
	*ptr++ = hexchars[29 >> 4];
	*ptr++ = hexchars[29 & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((unsigned char *)&ctx->regs.r[29], ptr, sizeof(u32));
	*ptr++ = ';';

	sprintf(ptr, "thread:%08x;", ctx->thid);
	ptr += strlen(ptr);

	if((ctx->regs.type == PSPLINK_EXTYPE_DEBUG) && 
			(g_context.daddr != 0) && (ctx->drcntl & (1 << 12)))
	{
		const char *type = NULL;

		switch(g_context.datatype)
		{
			case DATABP_TYPE_READ: type = "rwatch:";
								   break;
			case DATABP_TYPE_WRITE : type = "watch:";
									 break;
			case DATABP_TYPE_ACCESS: type = "awatch:";
									 break;
			default: break;
		};

		if(type)
		{
			strcpy(ptr, type);
			ptr += strlen(ptr);
			ptr = mem2hex((unsigned char *)&g_context.daddr, ptr, sizeof(u32));
			*ptr++ = ';';
		}
	}

	*ptr++ = 0;
}

static void handle_query(char *str)
{
	static SceUID threads[100];
	static int thread_count = 0;
	static int thread_loc = 0;

	switch(str[0])
	{
		case 'C': sprintf(output, "QC%08X", g_context.ctx.thid);
				  break;
		case 'f': 
				if(strncmp(str, "fThreadInfo", strlen("fThreadInfo")) == 0)
				{
					SceUID thread_temp[100];
					thread_count = sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, thread_temp, 100, &thread_count);
					if(thread_count > 0)
					{
						SceKernelThreadInfo info;
						int valid = 0;
						int i;

						for(i = 0; i < thread_count; i++)
						{
							memset(&info, 0, sizeof(info));
							info.size = sizeof(info);

							if(sceKernelReferThreadStatus(thread_temp[i], &info) == 0)
							{
								/* Check if this is a thread from our debugged application */
								if(((u32) info.entry >= g_context.info.text_addr) 
									&& ((u32) info.entry < (g_context.info.text_addr + g_context.info.text_size)))
								{
									threads[valid++] = thread_temp[i];
								}
							}
						}

						thread_count = valid;

						if(thread_count > 0)
						{
							thread_loc = 0;
							sprintf(output, "m%08X", threads[thread_loc]);
							thread_loc++;
						}
					}
				}
				break;

		case 's':
				if(strncmp(str, "sThreadInfo", strlen("sThreadInfo")) == 0)
				{
					if(thread_loc < thread_count)
					{
						sprintf(output, "m%08X", threads[thread_loc]);
						thread_loc++;
					}
					else
					{
						strcpy(output, "l");
					}
				}
				break;
		case 'T': if(strncmp(str, "ThreadExtraInfo,", strlen("ThreadExtraInfo,")) == 0)
				  {
					SceKernelThreadInfo info;
					SceUID thid;
					int i;

					str += strlen("ThreadExtraInfo,");
					if(hexToInt(&str, (unsigned int *) &thid))
					{
						memset(&info, 0, sizeof(info));
						info.size = sizeof(info);

						i = sceKernelReferThreadStatus(thid, &info);
						if(i == 0)
						{
							strtohex(output, info.name);
						}
						else
						{
							char temp[32];
							sprintf(temp, "Error: 0x%08X", i);
							strtohex(output, temp);
						}
					}
				  }
				  break;
		case 'P':
				break;
		case 'O': if(strncmp(str, "Offsets", strlen("Offsets")) == 0)
				  {
					  if(!g_context.elf)
					  {
						  u32 text;
						  u32 data;
						  u32 bss;

						  text = g_context.info.text_addr;
						  if(g_context.info.nsegment > 1)
						  {
							  data = g_context.info.segmentaddr[1];
						  }
						  else
						  {
							  data = text + g_context.info.text_size;
						  }
						  bss = data + g_context.info.data_size;

						  sprintf(output, "Text=%08X;Data=%08X;Bss=%08X", text, data, bss);
					  }
				  }
				  break;
	};
}

void handle_hwbp(char *str, int set)
{
	char *ptr;
	unsigned int addr;
	unsigned int len;
	struct DebugEnv env;
	int datatype = 0;

	if(g_context.hw == 0)
	{
		/* We dont have the hardware debugger on our side */
		return;
	}

	if((!isdigit(str[0])) || (str[1] != ','))
	{
		DEBUG_PRINTF("Invalid Z string (%s)\n", str);
		strcpy(output, "E01");
		return;
	}

	ptr = &str[2];

	if (hexToInt(&ptr, &addr) && *ptr++ == ',' && hexToInt(&ptr, &len))
   	{
		DEBUG_PRINTF("%c%c: addr 0x%08X, len 0x%08X\n", set ? 'Z' : 'z', str[0], addr, len);
		switch(str[0])
		{
			case '0': break;  /* We dont support inbuilt software breaks */
			case '1': if(set)
					  {
						  if(g_context.iaddr == 0)
						  {
							  debugGetEnv(&env);
							  env.IBA = addr;
							  env.IBAM = 0;
							  env.IBC = 2;
							  debugSetEnv(&env);
							  g_context.iaddr = addr;
							  strcpy(output, "OK");
						  }
						  else
						  {
							  strcpy(output, "E03");
						  }
					  }
					  else
					  {
						  if(g_context.iaddr != 0)
						  {
							  debugGetEnv(&env);
							  env.IBA = 0;
							  env.IBAM = 0;
							  env.IBC = 0;
							  debugSetEnv(&env);
							  g_context.iaddr = 0;
							  strcpy(output, "OK");
						  }
						  else
						  {
							  strcpy(output, "E03");
						  }
					  }
					  break;
			case '4': datatype++;
			case '2': datatype++;
			case '3': datatype++;
					  if(set)
					  {
						  if(g_context.daddr == 0)
						  {
							  debugGetEnv(&env);
							  env.DBA = addr;
							  env.DBAM = 0;
							  env.DBC = (datatype << 20) | 2;
							  debugSetEnv(&env);
							  g_context.daddr = addr;
							  g_context.datatype = datatype;
							  strcpy(output, "OK");
						  }
						  else
						  {
							  strcpy(output, "E03");
						  }
					  }
					  else
					  {
						  if(g_context.daddr != 0)
						  {
							  debugGetEnv(&env);
							  env.DBA = 0;
							  env.DBAM = 0;
							  env.DBC = 0;
							  debugSetEnv(&env);
							  g_context.daddr = 0;
							  g_context.datatype = 0;
							  strcpy(output, "OK");
						  }
						  else
						  {
							  strcpy(output, "E03");
						  }
					  }
					  break;
					  
			default:  break;
		};
	} 
	else
	{
		strcpy(output,"E01");
	}
}

int GdbHandleException (struct PsplinkContext *ctx)
{
	int ret = 1;
	int trap;
	int sigval;
	unsigned int addr;
	unsigned int length;
	char *ptr;
	int bflag = 0;

	DEBUG_PRINTF("In GDB Handle Exception\n");

	if(ctx->regs.type == PSPLINK_EXTYPE_DEBUG)
	{
		sigval = SIGTRAP;
	}
	else
	{
		trap = (ctx->regs.cause & 0x7c) >> 2;
		sigval = computeSignal(trap);
	}

	if(sigval == SIGHUP)
	{
		DEBUG_PRINTF("Trap %d\n", trap);
	}

	/* If step breakpoints set then put back the old values */
	if(g_stepbp[0].active)
	{
		_sw(g_stepbp[0].oldinst, g_stepbp[0].addr);
		g_stepbp[0].active = 0;
	}

	if(g_stepbp[1].active)
	{
		_sw(g_stepbp[1].oldinst, g_stepbp[1].addr);
		g_stepbp[1].active = 0;
	}

	if(g_context.started)
	{
		build_trap_cmd(sigval, ctx);
		putpacket((unsigned char *) output);
	}

	while(1)
	{
		if(!getpacket(input))
		{
			ret = 0;
			goto restart;
		}

		if((input[0] != 'X') && (input[0] != 'x'))
		{
			DEBUG_PRINTF("Received packet '%s'\n", input);
		}
		else
		{
			DEBUG_PRINTF("Received binary packet\n");
		}

		output[0] = 0;

		switch (input[0])
		{
		case '?':
			build_trap_cmd(sigval, ctx);
			break;

		case 'c':
			ptr = &input[1];
			if(!g_context.started)
			{
				int arglen = 0;
				int status;
				int i;

				for(i = 0; i < g_context.argc; i++)
				{
					arglen += strlen(g_context.argv[i]) + 1;
				}

				/* Ensure any pending memory is flushed before we start the module */
				sceKernelDcacheWritebackInvalidateAll();
				sceKernelIcacheInvalidateAll();
				sceKernelStartModule(g_context.uid, arglen, g_context.argv[0], &status, NULL);

				g_context.started = 1;
			}
			else
			{
				if (hexToInt(&ptr, &addr))
				{
					ctx->regs.epc = addr;
				}
			}
	  
			goto restart;
			break;

		case 'D':
			putpacket((unsigned char *) output);
			attached = 0;
			goto restart;
			break;

		case 'g':
			ptr = output;
			ptr = (char*) mem2hex((unsigned char *)&ctx->regs.r[0], ptr, 32*sizeof(u32)); /* r0...r31 */
			ptr = (char*) mem2hex((unsigned char *)&ctx->regs.status, ptr, 6*sizeof(u32)); /* cp0 */
			ptr = (char*) mem2hex((unsigned char *)&ctx->regs.fpr[0], ptr, 32*sizeof(u32)); /* f0...31 */
			ptr = (char*) mem2hex((unsigned char *)&ctx->regs.fsr, ptr, 2*sizeof(u32)); /* cp1 */
			ptr = (char*) mem2hex((unsigned char *)&ctx->regs.frame_ptr, ptr, 2*sizeof(u32)); /* frp */
			ptr = (char*) mem2hex((unsigned char *)&ctx->regs.index, ptr, 16*sizeof(u32)); /* cp0 */
			break;

		case 'G':
			ptr = &input[1];
			hex2mem(ptr, (char *)&ctx->regs.r[0], 32*sizeof(unsigned int), 0);
			ptr += 32*(2*sizeof(unsigned int));
			hex2mem(ptr, (char *)&ctx->regs.status, 6*sizeof(unsigned int), 0);
			ptr += 6*(2*sizeof(unsigned int));
			hex2mem(ptr, (char *)&ctx->regs.fpr[0], 32*sizeof(unsigned int), 0);
			ptr += 32*(2*sizeof(unsigned int));
			hex2mem(ptr, (char *)&ctx->regs.fsr, 2*sizeof(unsigned int), 0);
			ptr += 2*(2*sizeof(unsigned int));
			hex2mem(ptr, (char *)&ctx->regs.frame_ptr, 2*sizeof(unsigned int), 0);
			ptr += 2*(2*sizeof(unsigned int));
			hex2mem(ptr, (char *)&ctx->regs.index, 16*sizeof(unsigned int), 0);
			strcpy(output,"OK");
			break;

		/*
		 * mAA..AA,LLLL  Read LLLL bytes at address AA..AA
		 */
		case 'm':
			ptr = &input[1];

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)) {
				if (mem2hex((unsigned char *)addr, output, length))
					break;
				strcpy (output, "E03");
			} else
				strcpy(output,"E01");
			break;

		/*
		 * XAA..AA,LLLL: Write LLLL escaped binary bytes at address AA.AA
		 */
		case 'X':
			bflag = 1;
			/* fall through */

		/*
		 * MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK
		 */
		case 'M':
			ptr = &input[1];

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)
				&& *ptr++ == ':') {
				if (hex2mem(ptr, (char *)addr, length, bflag))
					strcpy(output, "OK");
				else
					strcpy(output, "E03");
			}
			else
				strcpy(output, "E02");
			bflag = 0;
			break;

		case 's': 	ptr = &input[1];
					if (hexToInt(&ptr, &addr))
					{
						ctx->regs.epc = addr;
					}

					step_generic(ctx, 0);
					goto restart;
					break;

		case 'q': handle_query(&input[1]);
				  break;

		case 'Z': handle_hwbp(&input[1], 1);
				  break;

		case 'z': handle_hwbp(&input[1], 0);
				  break;

		/*
		 * kill the program; let us try to restart the machine
		 * Reset the whole machine.
		 */
		case 'k':
		case 'r':	
				  sceKernelExitGame();
			break;

		default:
			break;
		}
		/*
		 * reply to the request
		 */

		putpacket((unsigned char *) output);

	} /* while */

restart:
	/* Flush caches */
	sceKernelDcacheWritebackInvalidateAll();
	sceKernelIcacheInvalidateAll();

	return ret;
}
