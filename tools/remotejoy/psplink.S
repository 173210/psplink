	.set noreorder

#include "pspstub.s"

	STUB_START "psplink",0x40090000,0x00170005
	STUB_FUNC  0x670C6041,psplinkPresent
	STUB_FUNC  0x811971CE,psplinkHandleException
	STUB_FUNC  0x8B5F450B,psplinkParseCommand
	STUB_FUNC  0xE3010EA1,psplinkPrintPrompt
	STUB_FUNC  0xB03642D9,psplinkExitShell
	STUB_FUNC  0x8F31ACE7,psplinkReferModule
	STUB_FUNC  0xA02A6B96,psplinkConsolePermit
	STUB_FUNC  0x94804B42,psplinkRegisterExceptions
	STUB_FUNC  0xB3277521,psplinkFindUserThreadContext
	STUB_FUNC  0xCBA8647D,apiHookByName
	STUB_FUNC  0x4ED59445,apiHookByNid
	STUB_FUNC  0xEFB166B7,debugEnableHW
	STUB_FUNC  0xDD3E47DE,debugDisableHW
	STUB_FUNC  0xEF83FB58,debugHWEnabled
	STUB_FUNC  0xD0A22864,debugGetEnv
	STUB_FUNC  0x4EC6E035,debugSetEnv
	STUB_FUNC  0x4DFA5010,ttySetWifiHandler
	STUB_FUNC  0x31F8AFD5,ttySetUsbHandler
	STUB_FUNC  0x753A27AC,ttySetConsHandler
	STUB_FUNC  0x79D1C3FA,sceKernelDcacheWritebackAll
	STUB_FUNC  0x864A9D72,sceKernelDcacheInvalidateAll
	STUB_FUNC  0xB435DEC5,sceKernelDcacheWritebackInvalidateAll
	STUB_FUNC  0x920F104A,sceKernelIcacheInvalidateAll
	STUB_END
