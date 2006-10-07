	.set noreorder

#include "pspimport.s"


#ifdef F_psplink_0000
	IMPORT_START "psplink",0x40090000
#endif
#ifdef F_psplink_0001
	IMPORT_FUNC  "psplink",0x670C6041,psplinkPresent
#endif
#ifdef F_psplink_0002
	IMPORT_FUNC  "psplink",0x811971CE,psplinkHandleException
#endif
#ifdef F_psplink_0003
	IMPORT_FUNC  "psplink",0x8B5F450B,psplinkParseCommand
#endif
#ifdef F_psplink_0004
	IMPORT_FUNC  "psplink",0xE3010EA1,psplinkPrintPrompt
#endif
#ifdef F_psplink_0005
	IMPORT_FUNC  "psplink",0xB03642D9,psplinkExitShell
#endif
#ifdef F_psplink_0006
	IMPORT_FUNC  "psplink",0x8F31ACE7,psplinkReferModule
#endif
#ifdef F_psplink_0007
	IMPORT_FUNC  "psplink",0xA02A6B96,psplinkConsolePermit
#endif
#ifdef F_psplink_0008
	IMPORT_FUNC  "psplink",0x94804B42,psplinkRegisterExceptions
#endif
#ifdef F_psplink_0009
	IMPORT_FUNC  "psplink",0xB3277521,psplinkFindUserThreadContext
#endif
#ifdef F_psplink_0010
	IMPORT_FUNC  "psplink",0x754F9BB8,psplinkReferModuleByName
#endif
#ifdef F_psplink_0011
	IMPORT_FUNC  "psplink",0x4A45386D,psplinkReset
#endif
#ifdef F_psplink_0012
	IMPORT_FUNC  "psplink",0xCBA8647D,apiHookByName
#endif
#ifdef F_psplink_0013
	IMPORT_FUNC  "psplink",0x4ED59445,apiHookByNid
#endif
#ifdef F_psplink_0014
	IMPORT_FUNC  "psplink",0xEFB166B7,debugEnableHW
#endif
#ifdef F_psplink_0015
	IMPORT_FUNC  "psplink",0xDD3E47DE,debugDisableHW
#endif
#ifdef F_psplink_0016
	IMPORT_FUNC  "psplink",0xEF83FB58,debugHWEnabled
#endif
#ifdef F_psplink_0017
	IMPORT_FUNC  "psplink",0xD0A22864,debugGetEnv
#endif
#ifdef F_psplink_0018
	IMPORT_FUNC  "psplink",0x4EC6E035,debugSetEnv
#endif
#ifdef F_psplink_0019
	IMPORT_FUNC  "psplink",0x4DFA5010,ttySetWifiHandler
#endif
#ifdef F_psplink_0020
	IMPORT_FUNC  "psplink",0x31F8AFD5,ttySetUsbHandler
#endif
#ifdef F_psplink_0021
	IMPORT_FUNC  "psplink",0x753A27AC,ttySetConsHandler
#endif
#ifdef F_psplink_0022
	IMPORT_FUNC  "psplink",0x79D1C3FA,sceKernelDcacheWritebackAll
#endif
#ifdef F_psplink_0023
	IMPORT_FUNC  "psplink",0x864A9D72,sceKernelDcacheInvalidateAll
#endif
#ifdef F_psplink_0024
	IMPORT_FUNC  "psplink",0xB435DEC5,sceKernelDcacheWritebackInvalidateAll
#endif
#ifdef F_psplink_0025
	IMPORT_FUNC  "psplink",0x920F104A,sceKernelIcacheInvalidateAll
#endif
