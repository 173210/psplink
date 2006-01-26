	.set noreorder

#include "pspstub.s"

	STUB_START "psplink",0x40090000,0x00060005
	STUB_FUNC  0x811971CE,psplinkHandleException
	STUB_FUNC  0x565C0B0E,sceKernelRegisterDefaultExceptionHandler
	STUB_FUNC  0x8B5F450B,psplinkParseCommand
	STUB_FUNC  0xE3010EA1,psplinkPrintPrompt
	STUB_FUNC  0xB03642D9,psplinkExitShell
	STUB_FUNC  0x3591BDA6,stdoutSetWifiHandler
	STUB_END
