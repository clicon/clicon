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
top_srcdir  	= ../..
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
CLICON_LIB	= libclicon.so.$(CLICON_MAJOR).$(CLICON_MINOR)

# Defined as "python" if python support is enabled, "none" otherwise
HAVE_PYTHON     = python

PYCLICON_MODDIR = $(DESTDIR)$(libdir)/clicon/python3.4

SRC             =  pyclicon.c pyclicon_db.c pyclicon_options.c \
	           pyclicon_log.c pyclicon_err.c
OBJS            = $(SRC:.c=.o)
SO_MODULE       = _clicon.so
PY_MODULE       = clicon.py

LIBPYCLICON     = ./lib/libpyclicon.a

# For dependency. A little strange that we rely on it being built in the src dir
# even though it may exist in $(libdir). But the new version may not have been installed yet.
LIBDEPS		= $(LIBPYCLICON) $(top_srcdir)/lib/src/$(CLICON_LIB) 

LIBS    	= $(LIBPYCLICON) -L$(top_srcdir)/lib/src -ldl -lnsl -lcrypt -lqdbm -l:libcligen.so.3 -lm -L /usr/local/lib  -l:$(CLICON_LIB) -lpthread -lpthread -ldl  -lutil -lm  -lpython3.4m
CPPFLAGS  	= -I/usr/local/include -DHAVE_CONFIG_H  -fPIC
INCLUDES	= -I. -I./lib -I$(top_srcdir)/lib -I$(top_srcdir)/include -I$(top_srcdir)  -I/usr/include/python3.4m -I/usr/include/python3.4m


SUBDIRS = lib cli backend

.PHONY: all clean depend install $(SUBDIRS)

all:	 $(HAVE_PYTHON) 

install: install-$(HAVE_PYTHON)

uninstall: uninstall-$(HAVE_PYTHON)


# Python not enabled
none:

install-none:

uninstall-none:


# Python support is enabled
python:	$(SUBDIRS) $(SO_MODULE) 

$(SUBDIRS):
	(cd $@; $(MAKE) $(MFLAGS) all)

$(SO_MODULE) : $(LIBPDEPS) $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LIBS)

install-python: $(HAVE_PYTHON)
	install -d $(DESTDIR)/$(PYCLICON_MODDIR)
	install $(PY_MODULE) $(SO_MODULE) $(DESTDIR)/$(PYCLICON_MODDIR)
	for i in $(SUBDIRS); do (cd $$i; $(MAKE) $(MFLAGS) install); done

uninstall-python:
	rm -f $(PYCLICON_MODDIR)/$(PY_MODULE) $(DESTDIR)/$(PYCLICON_MODDIR)/$(SO_MODULE)
	for i in $(SUBDIRS); do (cd $$i; $(MAKE) $(MFLAGS) uninstall); done

clean:
	rm -f $(OBJS) $(SO_MODULE) *.core
	for i in $(SUBDIRS); do (cd $$i; $(MAKE) $(MFLAGS) clean); done

distclean: clean
	rm -f Makefile *~ .de1pend
	for i in $(SUBDIRS); do (cd $$i; $(MAKE) $(MFLAGS) distclean); done

.SUFFIXES:
.SUFFIXES: .c .o

.c.o:
	$(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) -c $<


TAGS:
	find . -name '*.[chyl]' -print | etags -

depend:
	$(CC) $(DEPENDFLAGS) -DHAVE_CONFIG_H $(INCLUDES) $(CFLAGS) -MM $(SRC) > .depend

#include .depend

