#
# Asterisk -- A telephony toolkit for Linux.
# 
# Makefile for resource modules
#
# Copyright (C) 1999-2006, Digium, Inc.
#
# This program is free software, distributed under the terms of
# the GNU General Public License
#

-include $(ASTTOPDIR)/menuselect.makeopts $(ASTTOPDIR)/menuselect.makedeps

MODULE_PREFIX=res
MENUSELECT_CATEGORY=RES
MENUSELECT_DESCRIPTION=Resource Modules

all: _all

include $(ASTTOPDIR)/Makefile.moddir_rules

ifneq ($(findstring $(OSARCH), mingw32 cygwin ),)
  # cygwin has some dependencies among res_ things.
  # We use order-only dependencies, and then add the libraries as required.
  res_features.so: | res_monitor.so
  res_features.so_LIBS:= -lres_monitor.so
  #
  res_agi.so: | res_speech.so
  res_agi.so_LIBS:= -lres_speech.so
endif

res_config_ldap.o: _ASTCFLAGS+=-DLDAP_DEPRECATED

res_config_redis.o: ASTCFLAGS+=-I/usr/local/include/hiredis

$(if $(filter res_ais,$(EMBEDDED_MODS)),modules.link,res_ais.so): ais/clm.o ais/evt.o

$(if $(filter res_snmp,$(EMBEDDED_MODS)),modules.link,res_snmp.so): snmp/agent.o

clean::
	rm -f snmp/*.o snmp/*.i
