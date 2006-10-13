all:
	$(MAKE) -C libpsplink all
	$(MAKE) -C psplink	all
	$(MAKE) -C psplink_user all
	$(MAKE) -C gdbcommon all
	$(MAKE) -C modnet all
	$(MAKE) -C netshell all
	$(MAKE) -C netgdb   all
	$(MAKE) -C usbhostfs all
	$(MAKE) -C usbshell  all
	$(MAKE) -C usbgdb    all
	$(MAKE) -C conshell all
	$(MAKE) -C bootstrap all
	$(MAKE) -C bootstrap kxploit

release: all
	mkdir -p release/v1.0/psplink
	mkdir -p release/v1.5
	mkdir -p release/v1.5_nocorrupt
	mkdir -p release/pc
	cp -Rf scripts release/scripts
	cp bootstrap/EBOOT.PBP release/v1.0/psplink
	cp psplink/psplink.prx release/v1.0/psplink
	cp psplink/psplink.ini release/v1.0/psplink
	cp psplink/psplink.ini.usb release/v1.0/psplink
	cp psplink/psplink.ini.wifi release/v1.0/psplink
	cp psplink_user/psplink_user.prx release/v1.0/psplink
	cp modnet/modnet.prx release/v1.0/psplink
	cp netshell/netshell.prx release/v1.0/psplink
	cp netgdb/netgdb.prx release/v1.0/psplink
	cp usbhostfs/usbhostfs.prx release/v1.0/psplink
	cp usbshell/usbshell.prx release/v1.0/psplink
	cp conshell/conshell.prx release/v1.0/psplink
	cp usbgdb/usbgdb.prx release/v1.0/psplink
	cp -R bootstrap/psplink release/v1.5
	cp -R bootstrap/psplink% release/v1.5
	cp psplink/psplink.prx release/v1.5/psplink
	cp psplink/psplink.ini release/v1.5/psplink
	cp psplink/psplink.ini.usb release/v1.5/psplink
	cp psplink/psplink.ini.wifi release/v1.5/psplink
	cp psplink_user/psplink_user.prx release/v1.5/psplink
	cp modnet/modnet.prx release/v1.5/psplink
	cp netshell/netshell.prx release/v1.5/psplink
	cp netgdb/netgdb.prx release/v1.5/psplink
	cp usbhostfs/usbhostfs.prx release/v1.5/psplink
	cp usbshell/usbshell.prx release/v1.5/psplink
	cp conshell/conshell.prx release/v1.5/psplink
	cp usbgdb/usbgdb.prx release/v1.5/psplink
	cp -R release/v1.5/psplink release/v1.5_nocorrupt/__SCE__psplink
	cp -R release/v1.5/psplink% release/v1.5_nocorrupt/%__SCE__psplink
	cp -Rf pcterm release/pc
	cp -Rf usbhostfs_pc release/pc
	cp -Rf windows release/pc
	cp usbhostfs/usbhostfs.h release/pc/usbhostfs_pc
	cp README release
	cp LICENSE release
	cp psplink_manual.pdf release

clean:
	$(MAKE) -C libpsplink clean
	$(MAKE) -C psplink	clean
	$(MAKE) -C psplink_user clean
	$(MAKE) -C modnet clean
	$(MAKE) -C netshell clean
	$(MAKE) -C usbhostfs clean
	$(MAKE) -C usbshell clean
	$(MAKE) -C conshell clean
	$(MAKE) -C usbgdb   clean
	$(MAKE) -C netgdb   clean
	$(MAKE) -C gdbcommon clean
	$(MAKE) -C bootstrap clean
	rm -rf release
