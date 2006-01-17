# Patch module stop on the 1.5 firmware so you can stop modules which
# are not supposed to be stopped
pokew @sceModuleManager@+0x1350 0 0
pokew @sceModuleManager@+0x30 0
dcache wi
icache
