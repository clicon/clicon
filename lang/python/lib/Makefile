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

CPPFLAGS  	= -I/usr/local/include -DHAVE_CONFIG_H  -fPIC
INCLUDES	= -I. -I$(top_srcdir)/lib -I$(top_srcdir)/include -I$(top_srcdir)  -I/usr/include/python3.4m -I/usr/include/python3.4m

SRC         =  py23.c syspath.c handle.c
OBJS        = $(SRC:.c=.o)
LIB         = libpyclicon.a

all:	 $(LIB)

$(LIB):	$(OBJS) $(patsubst %,$(LIB)(%),$(OBJS))

install:

uninstall:

clean:
	rm -f $(OBJS) $(LIB) *.core

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

