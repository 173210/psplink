all:
	$(MAKE) -C psplink	all
	$(MAKE) -C psplink_user all
	$(MAKE) -C netshell all
	$(MAKE) -C bootstrap all
	$(MAKE) -C bootstrap kxploit

release: all
	mkdir -p release/v1.0/psplink
	mkdir -p release/v1.5
	cp bootstrap/EBOOT.PBP release/v1.0/psplink
	cp psplink/psplink.prx release/v1.0/psplink
	cp psplink/psplink.ini release/v1.0/psplink
	cp psplink_user/psplink_user.prx release/v1.0/psplink
	cp netshell/netshell.prx release/v1.0/psplink
	cp -R bootstrap/psplink release/v1.5
	cp -R bootstrap/psplink% release/v1.5
	cp psplink/psplink.prx release/v1.5/psplink
	cp psplink/psplink.ini release/v1.5/psplink
	cp psplink_user/psplink_user.prx release/v1.5/psplink
	cp netshell/netshell.prx release/v1.5/psplink
	cp README release
	cp LICENSE release

clean:
	$(MAKE) -C psplink	clean
	$(MAKE) -C psplink_user clean
	$(MAKE) -C netshell clean
	$(MAKE) -C bootstrap clean
	rm -rf release
