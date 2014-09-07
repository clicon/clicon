# 
# Makefile 
#
# Copyright (C) 2009-2014 Olof Hagsand and Benny Holmgren
#
# This file is part of CLICON.
#
# CLICON is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# CLICON is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with CLICON; see the file COPYING.  If not, see
# <http://www.gnu.org/licenses/>.
#
#


srcdir  	= .
top_srcdir  	= ../../..
CC		= gcc
CFLAGS  	= -g -Wall -Wall 
LDFLAGS 	= 

prefix 		= /usr/local
datarootdir	= ${prefix}/share
exec_prefix 	= ${prefix}
bindir 		= ${exec_prefix}/bin
libdir		= ${exec_prefix}/lib
mandir		= ${datarootdir}/man
libexecdir	= ${exec_prefix}/libexec
localstatedir	= ${prefix}/var
sysconfdir	= ${prefix}/etc
includedir	= ${prefix}/include

SH_SUFFIX	= .so
CLICON_MAJOR    = 2
CLICON_MINOR    = 3

# Use this clicon lib for linking
CLICON_LIB		= libclicon.so.$(CLICON_MAJOR).$(CLICON_MINOR)
CLICON_BACKEND_LIB	= libclicon_backend.so.$(CLICON_MAJOR).$(CLICON_MINOR)

PYCLICON_MODDIR     = $(DESTDIR)$(libdir)/clicon/python3.4
PYCLICON_BACKENDDIR = $(DESTDIR)$(libdir)/clicon/plugins/backend/

LIBPYCLICON     = ../lib/libpyclicon.a

# For dependency. A little strange that we rely on it being built in the src dir
# even though it may exist in $(libdir). But the new version may not have been installed yet.
LIBDEPS		= $(LIBPYCLICON) $(top_srcdir)/lib/src/$(CLICON_LIB) $(top_srcdir)/apps/config/$(CLICON_BACKEND_LIB) 

LIBS    	= $(LIBPYCLICON) -L$(top_srcdir)/lib/src -ldl -lnsl -lcrypt -lqdbm -l:libcligen.so.3 -lm -L /usr/local/lib -lpthread -lpthread -ldl  -lutil -lm  -lpython3.4m  -l:$(CLICON_BACKEND_LIB)  -l:$(CLICON_LIB)
CPPFLAGS  	= -I/usr/local/include -DHAVE_CONFIG_H  -fPIC -DPYCLICON_MODDIR=\"$(PYCLICON_MODDIR)\" -DPYCLICON_BACKENDDIR=\"$(PYCLICON_BACKENDDIR)\"
INCLUDES	= -I. -I../lib -I$(top_srcdir)/lib -I$(top_srcdir)/include -I$(top_srcdir)  -I/usr/include/python3.4m -I/usr/include/python3.4m


SRC             =  py_backend.c 
OBJS            = $(SRC:.c=.o)
SO_MODULE       = _cliconbackend.so
PY_MODULE       =  cliconbackend.py

all:	$(SO_MODULE)

$(SO_MODULE) : $(LIBDEPS) $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LIBS)

install:	all
	install -d $(DESTDIR)/$(PYCLICON_MODDIR)
	install -d $(DESTDIR)/$(PYCLICON_BACKENDDIR)
	install $(PY_MODULE) $(DESTDIR)/$(PYCLICON_MODDIR)
	install $(SO_MODULE) $(DESTDIR)/$(PYCLICON_BACKENDDIR)

uninstall:
	rm -f $(DESTDIR)/$(PYCLICON_MODDIR)/$(PY_MODULE) $(DESTDIR)/$(PYCLICON_BACKENDDIR)/$(SO_MODULE)

clean:
	rm -f $(OBJS) $(MODULE) *.core

distclean: clean
	rm -f Makefile *~ .depend

.SUFFIXES:
.SUFFIXES: .c .o

.c.o:
	$(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) -c $<


TAGS:
	find . -name '*.[chyl]' -print | etags -

depend:
	$(CC) $(DEPENDFLAGS) -DHAVE_CONFIG_H $(INCLUDES) $(CFLAGS) -MM $(SRC) > .depend

#include .depend

