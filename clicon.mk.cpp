# APPNAME and datadir must be defined

clicon_DBSPECDIR=prefix/share/$(APPNAME)
clicon_SYSCONFDIR=sysconfdir
clicon_LOCALSTATEDIR=localstatedir/$(APPNAME)
clicon_LIBDIR=libdir/$(APPNAME)
clicon_DATADIR=datadir/clicon


ifneq (,$(wildcard ${APPNAME}.conf.local)) 	
${APPNAME}.conf:  ${clicon_DATADIR}/clicon.conf.cpp ${APPNAME}.conf.local
	$(CPP) -P -x assembler-with-cpp -DAPPNAME=$(APPNAME) $< > $@
	cat ${APPNAME}.conf.local >> $@
else
${APPNAME}.conf:  ${clicon_DATADIR}/clicon.conf.cpp
	$(CPP) -P -x assembler-with-cpp -DAPPNAME=$(APPNAME) $< > $@
endif
