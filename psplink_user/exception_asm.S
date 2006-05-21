/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * exception_asm.S - Asm code for PSPLINK user exceptions
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

	.set noreorder
	.set noat

#define BadVAddr $8  // Address for the most recent address-related exception
#define Status   $12 // Processor status and control
#define Cause    $13 // Cause of last general exception
#define EPC      $14 // Program counter at last exception
#define PRId     $15 // Processor identification and revision

#define FSR		 $31
#define FIR      $0

#define REG_GPR_0    (6*4)
#define REG_GPR_1	 (REG_GPR_0 + 4)
#define REG_GPR_2	 (REG_GPR_1 + 4)
#define REG_GPR_3	 (REG_GPR_2 + 4)
#define REG_GPR_4	 (REG_GPR_3 + 4)
#define REG_GPR_5	 (REG_GPR_4 + 4)
#define REG_GPR_6	 (REG_GPR_5 + 4)
#define REG_GPR_7	 (REG_GPR_6 + 4)
#define REG_GPR_8	 (REG_GPR_7 + 4)
#define REG_GPR_9	 (REG_GPR_8 + 4)
#define REG_GPR_10	 (REG_GPR_9 + 4)
#define REG_GPR_11	 (REG_GPR_10 + 4)
#define REG_GPR_12	 (REG_GPR_11 + 4)
#define REG_GPR_13	 (REG_GPR_12 + 4)
#define REG_GPR_14	 (REG_GPR_13 + 4)
#define REG_GPR_15	 (REG_GPR_14 + 4)
#define REG_GPR_16	 (REG_GPR_15 + 4)
#define REG_GPR_17	 (REG_GPR_16 + 4)
#define REG_GPR_18	 (REG_GPR_17 + 4)
#define REG_GPR_19	 (REG_GPR_18 + 4)
#define REG_GPR_20	 (REG_GPR_19 + 4)
#define REG_GPR_21	 (REG_GPR_20 + 4)
#define REG_GPR_22	 (REG_GPR_21 + 4)
#define REG_GPR_23	 (REG_GPR_22 + 4)
#define REG_GPR_24	 (REG_GPR_23 + 4)
#define REG_GPR_25	 (REG_GPR_24 + 4)
#define REG_GPR_26	 (REG_GPR_25 + 4)
#define REG_GPR_27	 (REG_GPR_26 + 4)
#define REG_GPR_28	 (REG_GPR_27 + 4)
#define REG_GPR_29	 (REG_GPR_28 + 4)
#define REG_GPR_30	 (REG_GPR_29 + 4)
#define REG_GPR_31	 (REG_GPR_30 + 4)

#define REG_STATUS	 (REG_GPR_31 + 4)
#define REG_LO  	 (REG_STATUS + 4)
#define REG_HI	     (REG_LO + 4)
#define REG_BADVADDR (REG_HI + 4)
#define REG_CAUSE	 (REG_BADVADDR + 4)
#define REG_EPC	 	 (REG_CAUSE + 4)

#define REG_FPR_0 	 (REG_EPC + 4)
#define REG_FPR_1	 (REG_FPR_0 + 4)
#define REG_FPR_2	 (REG_FPR_1 + 4)
#define REG_FPR_3	 (REG_FPR_2 + 4)
#define REG_FPR_4	 (REG_FPR_3 + 4)
#define REG_FPR_5	 (REG_FPR_4 + 4)
#define REG_FPR_6	 (REG_FPR_5 + 4)
#define REG_FPR_7	 (REG_FPR_6 + 4)
#define REG_FPR_8	 (REG_FPR_7 + 4)
#define REG_FPR_9	 (REG_FPR_8 + 4)
#define REG_FPR_10	 (REG_FPR_9 + 4)
#define REG_FPR_11	 (REG_FPR_10 + 4)
#define REG_FPR_12	 (REG_FPR_11 + 4)
#define REG_FPR_13	 (REG_FPR_12 + 4)
#define REG_FPR_14	 (REG_FPR_13 + 4)
#define REG_FPR_15	 (REG_FPR_14 + 4)
#define REG_FPR_16	 (REG_FPR_15 + 4)
#define REG_FPR_17	 (REG_FPR_16 + 4)
#define REG_FPR_18	 (REG_FPR_17 + 4)
#define REG_FPR_19	 (REG_FPR_18 + 4)
#define REG_FPR_20	 (REG_FPR_19 + 4)
#define REG_FPR_21	 (REG_FPR_20 + 4)
#define REG_FPR_22	 (REG_FPR_21 + 4)
#define REG_FPR_23	 (REG_FPR_22 + 4)
#define REG_FPR_24	 (REG_FPR_23 + 4)
#define REG_FPR_25	 (REG_FPR_24 + 4)
#define REG_FPR_26	 (REG_FPR_25 + 4)
#define REG_FPR_27	 (REG_FPR_26 + 4)
#define REG_FPR_28	 (REG_FPR_27 + 4)
#define REG_FPR_29	 (REG_FPR_28 + 4)
#define REG_FPR_30	 (REG_FPR_29 + 4)
#define REG_FPR_31	 (REG_FPR_30 + 4)

#define REG_FSR		 (REG_FPR_31 + 4)
#define REG_FIR      (REG_FSR + 4)
#define REG_FP       (REG_FIR + 4)
#define REG_UNUSED1  (REG_FP  + 4)
#define REG_INDEX    (REG_UNUSED1 + 4)
#define REG_RANDOM   (REG_INDEX  + 4)
#define REG_ENTRYLO0 (REG_RANDOM + 4)
#define REG_ENTRYLO1 (REG_ENTRYLO0 + 4)
#define REG_CONTEXT  (REG_ENTRYLO1 + 4)
#define REG_PAGEMASK (REG_CONTEXT  + 4)
#define REG_WIRED    (REG_PAGEMASK + 4)
#define REG_COP0_7   (REG_WIRED + 4)
#define REG_COP0_8   (REG_COP0_7 + 4)
#define REG_COP0_9   (REG_COP0_8 + 4)
#define REG_ENTRYHI  (REG_COP0_9 + 4)
#define REG_COP0_11  (REG_ENTRYHI + 4)
#define REG_COP0_12  (REG_COP0_11 + 4)
#define REG_COP0_13  (REG_COP0_12 + 4)
#define REG_COP0_14  (REG_COP0_13 + 4)
#define REG_PRID     (REG_COP0_14 + 4)
#define REG_TYPE     (REG_PRID + 4)
#define REG_VFPU    (REG_TYPE + 4)

	.extern g_psplinkContext
	.extern psplinkTrap

	.global psplinkResumeFromException
	.ent    psplinkResumeFromException
psplinkResumeFromException:

	break

	.end    psplinkResumeFromException

	.global psplinkDefaultExHandler
	.ent    psplinkDefaultExHandler
psplinkDefaultExHandler:

	nop
	nop
	la		$v1, psplinkResumeFromException
	mfc0    $v0, EPC
	beq     $v0, $v1, _psplinkExceptionResume
	nop
# Set exception type to NORMAL
	la		$v1, _psplinkExType
	sw		$0, 0($v1)
	j		psplinkExceptionHandler
	nop

	.end    psplinkDefaultExHandler

	.global psplinkDebugExHandler
	.ent    psplinkDebugExHandler
psplinkDebugExHandler:
	nop
	nop
# Set exception type to DEBUG
	la		$v1, _psplinkExType
	li		$v0, 1
	sw		$v0, 0($v1)
	j		psplinkExceptionHandler
	nop
	.end    psplinkDebugExHandler

	.global psplinkExceptionHandler
	.ent    psplinkExceptionHandler
psplinkExceptionHandler:

# Scan the context array for a free handler, if we don't find one pass 
# NULL in $a0 so the thread can be terminated
	la		$v0, g_psplinkContext
1:
	# If we have no more context then exit
	beq     $v0, $0, 4f
	nop	

	lw		$v1, 0($v0)
	bnel	$v1, $0, 1b 
	lw		$v0, 4($v0)

	addiu	$v1, $0, 1
	sw		$v1, 0($v0)
	addiu	$v0, $v0, 8

	sw		$0, REG_GPR_0($v0)
	sw		$1, REG_GPR_1($v0)

	cfc0	$1, $4					# Get original v0
	sw		$1, REG_GPR_2($v0)
	cfc0 	$1, $5					# Get original v1
	sw		$1, REG_GPR_3($v0)
	sw		$4, REG_GPR_4($v0)
	sw		$5, REG_GPR_5($v0)
	sw		$6, REG_GPR_6($v0)
	sw		$7, REG_GPR_7($v0)
	sw		$8, REG_GPR_8($v0)
	sw		$9, REG_GPR_9($v0)
	sw		$10, REG_GPR_10($v0)
	sw		$11, REG_GPR_11($v0)
	sw		$12, REG_GPR_12($v0)
	sw		$13, REG_GPR_13($v0)
	sw		$14, REG_GPR_14($v0)
	sw		$15, REG_GPR_15($v0)
	sw		$16, REG_GPR_16($v0)
	sw		$17, REG_GPR_17($v0)
	sw		$18, REG_GPR_18($v0)
	sw		$19, REG_GPR_19($v0)
	sw		$20, REG_GPR_20($v0)
	sw		$21, REG_GPR_21($v0)
	sw		$22, REG_GPR_22($v0)
	sw		$23, REG_GPR_23($v0)
	sw		$24, REG_GPR_24($v0)
	sw		$25, REG_GPR_25($v0)
	sw		$26, REG_GPR_26($v0)
	sw		$27, REG_GPR_27($v0)
	sw		$28, REG_GPR_28($v0)
	sw		$29, REG_GPR_29($v0)
	sw		$30, REG_GPR_30($v0)
	sw		$31, REG_GPR_31($v0)

	mflo	$v1
	sw		$v1,  REG_LO($v0)
	mfhi	$v1
	sw		$v1,  REG_HI($v0)
	mfc0	$v1,  BadVAddr
	sw		$v1,  REG_BADVADDR($v0)
# Get cause
	cfc0	$v1,  $3
	sw		$v1,  REG_CAUSE($v0)
# Get EPC
	cfc0	$v1,  $0
	sw		$v1,  REG_EPC($v0)
# Get Status
	cfc0	$v1,  $2
	sw		$v1,  REG_STATUS($v0)

# Copy in the exception type
	la		$a0,  _psplinkExType
	lw		$a0,  0($a0)
	sw		$a0,  REG_TYPE($v0)

# Check if cop1 is enabled and skip if not
	lui		$a0, 0x2000
	and		$a0, $a0, $v1
	beq		$a0, $0, 2f
	nop

	swc1		$0, REG_FPR_0($v0)
	swc1		$1, REG_FPR_1($v0)
	swc1		$2, REG_FPR_2($v0)
	swc1		$3, REG_FPR_3($v0)
	swc1		$4, REG_FPR_4($v0)
	swc1		$5, REG_FPR_5($v0)
	swc1		$6, REG_FPR_6($v0)
	swc1		$7, REG_FPR_7($v0)
	swc1		$8, REG_FPR_8($v0)
	swc1		$9, REG_FPR_9($v0)
	swc1		$10, REG_FPR_10($v0)
	swc1		$11, REG_FPR_11($v0)
	swc1		$12, REG_FPR_12($v0)
	swc1		$13, REG_FPR_13($v0)
	swc1		$14, REG_FPR_14($v0)
	swc1		$15, REG_FPR_15($v0)
	swc1		$16, REG_FPR_16($v0)
	swc1		$17, REG_FPR_17($v0)
	swc1		$18, REG_FPR_18($v0)
	swc1		$19, REG_FPR_19($v0)
	swc1		$20, REG_FPR_20($v0)
	swc1		$21, REG_FPR_21($v0)
	swc1		$22, REG_FPR_22($v0)
	swc1		$23, REG_FPR_23($v0)
	swc1		$24, REG_FPR_24($v0)
	swc1		$25, REG_FPR_25($v0)
	swc1		$26, REG_FPR_26($v0)
	swc1		$27, REG_FPR_27($v0)
	swc1		$28, REG_FPR_28($v0)
	swc1		$29, REG_FPR_29($v0)
	swc1		$30, REG_FPR_30($v0)
	swc1		$31, REG_FPR_31($v0)

	cfc1 		$t0, FSR
	sw			$t0, REG_FSR($v0)
	cfc1		$t0, FIR
	sw			$t0, REG_FIR($v0)
	ctc1		$0, FSR			# Clear any cause flags

# Jump target for ignore cop1
2:

# Check for VFPU enable
	cfc0	$v1,  $2
	lui		$a0, 0x4000
	and		$a0, $a0, $v1
	beq		$a0, $0, 3f
	nop

# Store VFPU registers
	usv.q	C000, REG_VFPU($v0)
	usv.q	C010, REG_VFPU+16($v0)
	usv.q	C020, REG_VFPU+32($v0)
	usv.q	C030, REG_VFPU+48($v0)
	usv.q	C100, REG_VFPU+64($v0)
	usv.q	C110, REG_VFPU+80($v0)
	usv.q	C120, REG_VFPU+96($v0)
	usv.q	C130, REG_VFPU+112($v0)
	usv.q	C200, REG_VFPU+128($v0)
	usv.q	C210, REG_VFPU+144($v0)
	usv.q	C220, REG_VFPU+160($v0)
	usv.q	C230, REG_VFPU+176($v0)
	usv.q	C300, REG_VFPU+192($v0)
	usv.q	C310, REG_VFPU+208($v0)
	usv.q	C320, REG_VFPU+224($v0)
	usv.q	C330, REG_VFPU+240($v0)
	usv.q	C400, REG_VFPU+256($v0)
	usv.q	C410, REG_VFPU+272($v0)
	usv.q	C420, REG_VFPU+288($v0)
	usv.q	C430, REG_VFPU+304($v0)
	usv.q	C500, REG_VFPU+320($v0)
	usv.q	C510, REG_VFPU+336($v0)
	usv.q	C520, REG_VFPU+352($v0)
	usv.q	C530, REG_VFPU+368($v0)
	usv.q	C600, REG_VFPU+384($v0)
	usv.q	C610, REG_VFPU+400($v0)
	usv.q	C620, REG_VFPU+416($v0)
	usv.q	C630, REG_VFPU+432($v0)
	usv.q	C700, REG_VFPU+448($v0)
	usv.q	C710, REG_VFPU+464($v0)
	usv.q	C720, REG_VFPU+480($v0)
	usv.q	C730, REG_VFPU+496($v0)

# Jump target for ignore cop2
3:
	sw			$sp, REG_FP($v0)
	addiu		$v0, $v0, -8


4:
	move	$a0, $v0
	la		$v0, psplinkTrap
	mtc0	$v0, EPC 
	nop
	nop
	eret
	nop
	nop

	.end psplinkExceptionHandler

	.ent _psplinkExceptionResume

_psplinkExceptionResume:

	move	$v0, $a0
	sw		$0, 0($v0)
	addiu	$v0, $v0, 8

	lw		$v1,  REG_STATUS($v0)

# Check if cop2 is enabled and skip if not
	lui		$a0, 0x4000
	and		$a0, $a0, $v1
	beq		$a0, $0, 1f
	nop

# Restore VFPU registers
	ulv.q	C000, REG_VFPU($v0)
	ulv.q	C010, REG_VFPU+16($v0)
	ulv.q	C020, REG_VFPU+32($v0)
	ulv.q	C030, REG_VFPU+48($v0)
	ulv.q	C100, REG_VFPU+64($v0)
	ulv.q	C110, REG_VFPU+80($v0)
	ulv.q	C120, REG_VFPU+96($v0)
	ulv.q	C130, REG_VFPU+112($v0)
	ulv.q	C200, REG_VFPU+128($v0)
	ulv.q	C210, REG_VFPU+144($v0)
	ulv.q	C220, REG_VFPU+160($v0)
	ulv.q	C230, REG_VFPU+176($v0)
	ulv.q	C300, REG_VFPU+192($v0)
	ulv.q	C310, REG_VFPU+208($v0)
	ulv.q	C320, REG_VFPU+224($v0)
	ulv.q	C330, REG_VFPU+240($v0)
	ulv.q	C400, REG_VFPU+256($v0)
	ulv.q	C410, REG_VFPU+272($v0)
	ulv.q	C420, REG_VFPU+288($v0)
	ulv.q	C430, REG_VFPU+304($v0)
	ulv.q	C500, REG_VFPU+320($v0)
	ulv.q	C510, REG_VFPU+336($v0)
	ulv.q	C520, REG_VFPU+352($v0)
	ulv.q	C530, REG_VFPU+368($v0)
	ulv.q	C600, REG_VFPU+384($v0)
	ulv.q	C610, REG_VFPU+400($v0)
	ulv.q	C620, REG_VFPU+416($v0)
	ulv.q	C630, REG_VFPU+432($v0)
	ulv.q	C700, REG_VFPU+448($v0)
	ulv.q	C710, REG_VFPU+464($v0)
	ulv.q	C720, REG_VFPU+480($v0)
	ulv.q	C730, REG_VFPU+496($v0)

1:

	lw		$v1,  REG_STATUS($v0)

# Check if cop1 is enabled and skip if not
	lui		$a0, 0x2000
	and		$a0, $a0, $v1
	beq		$a0, $0, 2f
	nop

	lwc1		$0, REG_FPR_0($v0)
	lwc1		$1, REG_FPR_1($v0)
	lwc1		$2, REG_FPR_2($v0)
	lwc1		$3, REG_FPR_3($v0)
	lwc1		$4, REG_FPR_4($v0)
	lwc1		$5, REG_FPR_5($v0)
	lwc1		$6, REG_FPR_6($v0)
	lwc1		$7, REG_FPR_7($v0)
	lwc1		$8, REG_FPR_8($v0)
	lwc1		$9, REG_FPR_9($v0)
	lwc1		$10, REG_FPR_10($v0)
	lwc1		$11, REG_FPR_11($v0)
	lwc1		$12, REG_FPR_12($v0)
	lwc1		$13, REG_FPR_13($v0)
	lwc1		$14, REG_FPR_14($v0)
	lwc1		$15, REG_FPR_15($v0)
	lwc1		$16, REG_FPR_16($v0)
	lwc1		$17, REG_FPR_17($v0)
	lwc1		$18, REG_FPR_18($v0)
	lwc1		$19, REG_FPR_19($v0)
	lwc1		$20, REG_FPR_20($v0)
	lwc1		$21, REG_FPR_21($v0)
	lwc1		$22, REG_FPR_22($v0)
	lwc1		$23, REG_FPR_23($v0)
	lwc1		$24, REG_FPR_24($v0)
	lwc1		$25, REG_FPR_25($v0)
	lwc1		$26, REG_FPR_26($v0)
	lwc1		$27, REG_FPR_27($v0)
	lwc1		$28, REG_FPR_28($v0)
	lwc1		$29, REG_FPR_29($v0)
	lwc1		$30, REG_FPR_30($v0)
	lwc1		$31, REG_FPR_31($v0)

	lw			$t0, REG_FSR($v0)
	li			$t1, 0xFFFC0F83
	and			$t0, $t0, $t1	# Clear the cause and flags before ret
	ctc1 		$t0, FSR
2:

# Don't do 1 or 2 yet
	lw		$3, REG_GPR_3($v0)
	lw		$4, REG_GPR_4($v0)
	lw		$5, REG_GPR_5($v0)
	lw		$6, REG_GPR_6($v0)
	lw		$7, REG_GPR_7($v0)
	lw		$8, REG_GPR_8($v0)
	lw		$9, REG_GPR_9($v0)
	lw		$10, REG_GPR_10($v0)
	lw		$11, REG_GPR_11($v0)
	lw		$12, REG_GPR_12($v0)
	lw		$13, REG_GPR_13($v0)
	lw		$14, REG_GPR_14($v0)
	lw		$15, REG_GPR_15($v0)
	lw		$16, REG_GPR_16($v0)
	lw		$17, REG_GPR_17($v0)
	lw		$18, REG_GPR_18($v0)
	lw		$19, REG_GPR_19($v0)
	lw		$20, REG_GPR_20($v0)
	lw		$21, REG_GPR_21($v0)
	lw		$22, REG_GPR_22($v0)
	lw		$23, REG_GPR_23($v0)
	lw		$24, REG_GPR_24($v0)
	lw		$25, REG_GPR_25($v0)
	lw		$26, REG_GPR_26($v0)
	lw		$27, REG_GPR_27($v0)
	lw		$28, REG_GPR_28($v0)
	lw		$29, REG_GPR_29($v0)
	lw		$30, REG_GPR_30($v0)
	lw		$31, REG_GPR_31($v0)

	lw		$1,  REG_EPC($v0)
	mtc0    $1,  EPC

# Restore 1 + 2 now
	lw		$1, REG_GPR_1($v0)
	lw		$2, REG_GPR_2($v0)

# When resuming debug exceptions take the value of status and make a decision
# whether to resume in user or kernel modes

	nop
	nop
	eret

	.end	_psplinkExceptionResume

	.data

	.global _psplinkExType
_psplinkExType:
	.word	0
