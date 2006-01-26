	.set noreorder

#include "pspstub.s"

	STUB_START "psplinkUser",0x00090000,0x00020005
	STUB_FUNC  0xBAC30B81,psplinkUserRegisterGdbHandler
	STUB_FUNC  0x52B514CE,pspDebugScreenPrintf
	STUB_END
