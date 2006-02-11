	.set noreorder

#include "pspstub.s"

	STUB_START "psplink",0x40090000,0x000C0005
	STUB_FUNC  0x670C6041,psplinkPresent
	STUB_FUNC  0x811971CE,psplinkHandleException
	STUB_FUNC  0x8B5F450B,psplinkParseCommand
	STUB_FUNC  0xE3010EA1,psplinkPrintPrompt
	STUB_FUNC  0xB03642D9,psplinkExitShell
	STUB_FUNC  0x8F31ACE7,psplinkReferModule
	STUB_FUNC  0x4DFA5010,ttySetWifiHandler
	STUB_FUNC  0x565C0B0E,sceKernelRegisterDefaultExceptionHandler
	STUB_FUNC  0x79D1C3FA,sceKernelDcacheWritebackAll
	STUB_FUNC  0x864A9D72,sceKernelDcacheInvalidateAll
	STUB_FUNC  0xB435DEC5,sceKernelDcacheWritebackInvalidateAll
	STUB_FUNC  0x920F104A,sceKernelIcacheInvalidateAll
	STUB_END
