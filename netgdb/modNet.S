	.set noreorder

#include "pspstub.s"

	STUB_START "modNet",0x00090000,0x00020005
	STUB_FUNC  0xFB177E7C,modNetIsInit
	STUB_FUNC  0xE013D162,modNetGetIpAddress
	STUB_END
