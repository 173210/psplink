	.set noreorder

#include "pspstub.s"

	STUB_START "sdkbridge",0x40090000,0x00060005
	STUB_FUNC  0x141FEA7E,pspDebugSioPutchar
	STUB_FUNC  0xE4F2196D,pspDebugSioGetchar
	STUB_FUNC  0x84F370BC,Kprintf
	STUB_FUNC  0x565C0B0E,sceKernelRegisterDefaultExceptionHandler
	STUB_FUNC  0x8E982A74,sceIoAddDrv
	STUB_FUNC  0xC7F35804,sceIoDelDrv
	STUB_END
