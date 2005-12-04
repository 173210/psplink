	.set noreorder

#include "pspstub.s"

	STUB_START "psplink",0x40090000,0x00020005
	STUB_FUNC  0x3D308221,psplinkInitialised
	STUB_FUNC  0x811971CE,psplinkHandleException
	STUB_END
