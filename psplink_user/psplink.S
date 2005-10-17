	.set noreorder

#include "pspstub.s"

	STUB_START "psplink",0x40090000,0x00060005
	STUB_FUNC  0x3D308221,psplinkInitialised
	STUB_FUNC  0x811971CE,psplinkHandleException
	STUB_FUNC  0x141FEA7E,pspDebugSioPutchar
	STUB_FUNC  0xE4F2196D,pspDebugSioGetchar
	STUB_FUNC  0x84F370BC,Kprintf
	STUB_FUNC  0x565C0B0E,sceKernelRegisterDefaultExceptionHandler
	STUB_END
