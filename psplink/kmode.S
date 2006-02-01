#include "as_reg_compat.h"

	.set noreorder
	.set noat

	.global psplinkSetK1
	.ent    psplinkSetK1

psplinkSetK1:
	move $v0, $k1
	jr	 $ra
	move $k1, $a0

	.end psplinkSetK1

	.global psplinkGetCop0
	.ent    psplinkGetCop0

psplinkGetCop0:
	mfc0	$v0, $0
	sw		$v0, 0($a0)
	mfc0	$v0, $1
	sw		$v0, 4($a0)
	mfc0	$v0, $2
	sw		$v0, 8($a0)
	mfc0	$v0, $3
	sw		$v0, 12($a0)
	mfc0	$v0, $4
	sw		$v0, 16($a0)
	mfc0	$v0, $5
	sw		$v0, 20($a0)
	mfc0	$v0, $6
	sw		$v0, 24($a0)
	mfc0	$v0, $7
	sw		$v0, 28($a0)
	mfc0	$v0, $8
	sw		$v0, 32($a0)
	mfc0	$v0, $9
	sw		$v0, 36($a0)
	mfc0	$v0, $10
	sw		$v0, 40($a0)
	mfc0	$v0, $11
	sw		$v0, 44($a0)
	mfc0	$v0, $12
	sw		$v0, 48($a0)
	mfc0	$v0, $13
	sw		$v0, 52($a0)
	mfc0	$v0, $14
	sw		$v0, 56($a0)
	mfc0	$v0, $15
	sw		$v0, 60($a0)
	mfc0	$v0, $16
	sw		$v0, 64($a0)
	mfc0	$v0, $17
	sw		$v0, 68($a0)
	mfc0	$v0, $18
	sw		$v0, 72($a0)
	mfc0	$v0, $19
	sw		$v0, 76($a0)
	mfc0	$v0, $20
	sw		$v0, 80($a0)
	mfc0	$v0, $21
	sw		$v0, 84($a0)
	mfc0	$v0, $22
	sw		$v0, 88($a0)
	mfc0	$v0, $23
	sw		$v0, 92($a0)
	mfc0	$v0, $24
	sw		$v0, 96($a0)
	mfc0	$v0, $25
	sw		$v0, 100($a0)
	mfc0	$v0, $26
	sw		$v0, 104($a0)
	mfc0	$v0, $27
	sw		$v0, 108($a0)
	mfc0	$v0, $28
	sw		$v0, 112($a0)
	mfc0	$v0, $29
	sw		$v0, 116($a0)
	mfc0	$v0, $30
	sw		$v0, 120($a0)
	mfc0	$v0, $31
	sw		$v0, 124($a0)
	addiu	$a0, $a0, 128
	cfc0	$v0, $0
	sw		$v0, 0($a0)
	cfc0	$v0, $1
	sw		$v0, 4($a0)
	cfc0	$v0, $2
	sw		$v0, 8($a0)
	cfc0	$v0, $3
	sw		$v0, 12($a0)
	cfc0	$v0, $4
	sw		$v0, 16($a0)
	cfc0	$v0, $5
	sw		$v0, 20($a0)
	cfc0	$v0, $6
	sw		$v0, 24($a0)
	cfc0	$v0, $7
	sw		$v0, 28($a0)
	cfc0	$v0, $8
	sw		$v0, 32($a0)
	cfc0	$v0, $9
	sw		$v0, 36($a0)
	cfc0	$v0, $10
	sw		$v0, 40($a0)
	cfc0	$v0, $11
	sw		$v0, 44($a0)
	cfc0	$v0, $12
	sw		$v0, 48($a0)
	cfc0	$v0, $13
	sw		$v0, 52($a0)
	cfc0	$v0, $14
	sw		$v0, 56($a0)
	cfc0	$v0, $15
	sw		$v0, 60($a0)
	cfc0	$v0, $16
	sw		$v0, 64($a0)
	cfc0	$v0, $17
	sw		$v0, 68($a0)
	cfc0	$v0, $18
	sw		$v0, 72($a0)
	cfc0	$v0, $19
	sw		$v0, 76($a0)
	cfc0	$v0, $20
	sw		$v0, 80($a0)
	cfc0	$v0, $21
	sw		$v0, 84($a0)
	cfc0	$v0, $22
	sw		$v0, 88($a0)
	cfc0	$v0, $23
	sw		$v0, 92($a0)
	cfc0	$v0, $24
	sw		$v0, 96($a0)
	cfc0	$v0, $25
	sw		$v0, 100($a0)
	cfc0	$v0, $26
	sw		$v0, 104($a0)
	cfc0	$v0, $27
	sw		$v0, 108($a0)
	cfc0	$v0, $28
	sw		$v0, 112($a0)
	cfc0	$v0, $29
	sw		$v0, 116($a0)
	cfc0	$v0, $30
	sw		$v0, 120($a0)
	cfc0	$v0, $31
	sw		$v0, 124($a0)

	.end psplinkGetCop0
