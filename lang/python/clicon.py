#
#  PyCLICON module
#
# Copyright (C) 2009-2015 Olof Hagsand & Benny Holmgren
#
#  This file is part of CLICON.
#
#  PyCLICON is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  PyCLICON is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with PyCLICON; see the file LICENSE.

"""Python bindings for the CLICON software suite

This module implements a Python API to CLICON, allowing the developer
to utilize the power of CLICON without coding in C.

CLICON is a software suite for configuration management including CLI gen-
eration, netconf interface and embedded databases. White-box systems, em-
bedded devices and other systems can with a few steps add a conistent man-
agement interface with CLI, netconf access, realtime-database and transactions
support.

"""
# Make python2 behave more like python 3.
from __future__ import unicode_literals, absolute_import, print_function

__version__ = '0.1'

#import sys
import inspect
import cligen 
import _clicon
from _clicon import *

CLICON_CANDIDATE_DB	= 'CLICON_CANDIDATE_DB'
CLICON_RUNNING_DB	= 'CLICON_RUNNING_DB'
CLICON_APPDIR		= 'CLICON_APPDIR'
CLICON_CONFIGFILE	= 'CLICON_CONFIGFILE'
CLICON_DBSPEC_TYPE	= 'CLICON_DBSPEC_TYPE'
CLICON_YANG_DIR		= 'CLICON_YANG_DIR'
CLICON_YANG_MODULE_MAIN	= 'CLICON_YANG_MODULE_MAIN'
CLICON_BACKEND_DIR	= 'CLICON_BACKEND_DIR'
CLICON_CLI_DIR		= 'CLICON_CLI_DIR'
CLICON_CLISPEC_DIR	= 'CLICON_CLISPEC_DIR'
CLICON_NETCONF_DIR	= 'CLICON_NETCONF_DIR'
CLICON_ARCHIVE_DIR	= 'CLICON_ARCHIVE_DIR'
CLICON_STARTUP_CONFIG	= 'CLICON_STARTUP_CONFIG'
CLICON_SOCK		= 'CLICON_SOCK'
CLICON_BACKEND_PIDFILE	= 'CLICON_BACKEND_PIDFILE'
CLICON_SOCK_GROUP	= 'CLICON_SOCK_GROUP'
CLICON_MASTER_PLUGIN	= 'CLICON_MASTER_PLUGIN'
CLICON_CLI_MODE		= 'CLICON_CLI_MODE'
CLICON_AUTOCOMMIT	= 'CLICON_AUTOCOMMIT'



def clicon_log(level, msg):
    return _clicon._clicon_log(level, msg)

def clicon_debug(level, msg):
    return _clicon._clicon_debug(level, msg)

def clicon_err(level, err, msg):
    (frame, filename, line, func, lines, index) = \
                            inspect.getouterframes(inspect.currentframe())[1]
    return _clicon._clicon_err(func, line, level, err, msg)

def clicon_strerror(err):
    return _clicon._clicon_strerror(err)

def clicon_err_reset():
    return _clicon._clicon_err_reset()

class Options:
    'Clicon options object'

    def __init__(self, h):
        self._h = h
    
    def __getitem__(self, key):
        return _clicon._clicon_option(self._h, key)

    def __setitem__(self, key, val):
        return _clicon._clicon_option_set(self._h, key, str(val))

    def __delitem__(self, key):
        return _clicon._clicon_option_del(self._h, key)

    def __contains__(self, key):
        return _clicon._clicon_option_exists(self._h, key)

    def __iter__(self):
        for key in self.keys():
            yield key

    def keys(self):
        return _clicon._clicon_options(self._h)

    def items(self):
        itm = dict()
        for key in self:
            itm[key] = self[key]
        return itm
        
class BaseHandle:
    'A base handle object'

    def __init__(self, h):
        self._h = h
        self.options = Options(self._h)


    def candidate_db(self):
        return self.options[CLICON_CANDIDATE_DB]

    def running_db(self):
        return self.options[CLICON_RUNNING_DB]

    def appdir(self):
        return self.options[CLICON_APPDIR]

    def config_file(self):
        return self.options[CLICON_CONFIGFILE]

    def dbspec_type(self):
        return self.options[CLICON_DBSPEC_TYPE]

    def yang_dir(self):
        return self.options[CLICON_YANG_DIR]

    def yang_module_main(self):
        return self.options[CLICON_YANG_MODULE_MAIN]

    def backend_dir(self):
        return self.options[CLICON_BACKEND_DIR]

    def cli_dir(self):
        return self.options[CLICON_CLI_DIR]

    def clispec_dir(self):
        return self.options[CLICON_CLISPEC_DIR]

    def netconf_dir(self):
        return self.options[CLICON_NETCONF_DIR]

    def archive_dir(self):
        return self.options[CLICON_ARCHIVE_DIR]

    def startup_config(self):
        return self.options[CLICON_STARTUP_CONFIG]

    def sock(self):
        return self.options[CLICON_SOCK]

    def backend_pidfile(self):
        return self.options[CLICON_BACKEND_PIDFILE]

    def sock_group(self):
        return self.options[CLICON_SOCK_GROUP]

    def master_plugin(self):
        return self.options[CLICON_MASTER_PLUGIN]

    def cli_mode(self):
        return self.options[CLICON_CLI_MODE]

    def autocommit(self):
        return self.options[CLICON_AUTOCOMMIT]

class CliconDB(_clicon._CliconDB):
    'A reference to a CLICON database'

    def __init__(self, filename):
        return super(CliconDB, self).__init__(filename)
    

    def __getitem__(self, key):
        return self.get(key)

    def __setitem__(self, key, val):
        return self.put(key, val)

    def __delitem__(self, key):
        return self.delete(key)

    def __contains__(self, key):
        if self.get(key) is not None:
            return True
        return False

    def __iter__(self):
        for key in self.keys():
            yield key

    def keys(self, rx='.*'):
        return super(CliconDB, self)._keys(rx)

    def put(self, key, val):
        if isinstance(val, cligen.CgVar):
            cvec = self.get(key)
            if cvec is None:
                cvec = clicon.Cvec()
            if val.name_get() in cvec:
                cvec[val.name_get()] = val
            else:
                cvec += val
                
        elif isinstance(val, cligen.Cvec):
            cvec = val
        else:
            raise TypeError("argument 2 must be clicon.CgVar or clicon.Cvec")
            
        return self._cvec2db(key, cvec)


    def append(self, key, val):

        cvec = self.get(key)
        if cvec is None:
            cvec = cligen.Cvec()

        if isinstance(val, cligen.CgVar) or isinstance(val, cligen.Cvec):
            cvec += val
        else:
            raise TypeError("aregument 2 must be clicon.CgVar or clicon.Cvec")

        return self._cvec2db(key, cvec)
        

    def get(self, key=None, var=None):
        if var is not None:
            return self._dbvar2cv(key, var)
        else:
            return self._dbkey2cvec(key)


    def items(self, rx='.*'):
        return super(CliconDB, self)._items(rx)

    def delete(self, key):
        return super(CliconDB, self)._db_del(key)


    def db2txt(self, handle, *args, **kwargs):
        """
   Args:
     handle:  The CLICON handle
     format:  A string containing the db2txt format or a keyword argument
              specifying an input format or file. For example>
                 syntax="set hostname $system.hostname->hostname"
                    or
                 file="/my/input/file.d2t"

   Raises:
      TypeError:    If invalid arguments are provided.
      MemoryError:  If memory allcoation fails
      ValueError:   If the format passed cannot be parsed successfully
      """
        numargs = len(args) + len(kwargs)
        if numargs is not 1:
            raise TypeError("function takes 2 arguments ({:d} given)".format(numargs)+1)

        if "file" in kwargs:
            with open(kwargs['file'], 'r') as f:
                syntax = f.read()
        elif "syntax" in kwargs:
            syntax = kwargs['syntax']
        elif len(args) is 1:
            syntax = args[0]
        else:
            raise TypeError("Invalid syntax argument")

        return super(CliconDB, self)._db2txt(handle._h, syntax)
