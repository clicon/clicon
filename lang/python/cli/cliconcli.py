#
#  PyCLICON CLI module
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
import os
import glob
import importlib
import cligen 
from clicon import *
from _cliconcli import *

# Constants
PLUGIN_INIT      = 'plugin_init'
PLUGIN_START     = 'plugin_start'
PLUGIN_EXIT      = 'plugin_exit'
PLUGIN_RESET     = 'plugin_reset'
PLUGIN_BEGIN     = 'transaction_begin'
PLUGIN_COMPLETE  = 'transaction_complete'
PLUGIN_END       = 'transaction_end'
PLUGIN_ABORT     = 'transaction_abort'

# A global dict with plugin info
__plugins__ = {}  
# A global dict of dependencies
__dbdeps__ = {}


class Handle(BaseHandle):
    pass



class Plugin:
    
    def __init__(self, handle, name):
        self._name = name
        self._plugin = importlib.import_module(name)
        self._init     = _find_method(self._plugin, PLUGIN_INIT)
        self._start    = _find_method(self._plugin, PLUGIN_START)
        self._exit     = _find_method(self._plugin, PLUGIN_EXIT)

    def __str__(self):
        return self._name

    def __repr__(self):
        return self._name




def _find_method(ob, name):
    if hasattr(ob, name):
        return getattr(ob,name)
    else:
        return None


def _plugin_init(h):
    global __plugins__

    handle = Handle(h)
    for path in glob.glob(handle.cli_dir()+"/*.py"):
        name = os.path.basename(path)[0:-3]
        try:
            __plugins__[name] = Plugin(handle, name)
            if (__plugins__[name]._init(handle) < 0):
                return -1;

        except Exception as e:
            print("Python plugin '{:s}' failed to load: {:s}\n".format(name, str(e)))
            clicon_err(OE_FATAL, 0,
                       "Python plugin '{:s}' failed to load: {:s}\n".format(name, str(e)))
            return -1

    return len(__plugins__)

def _plugin_start(h, argc, argv):
    global __plugins__

    handle = Handle(h)
    for name, p in __plugins__.items():
        if p._start is not None:
            clicon_debug(1, "Calling {:s}.plugin_start()\n".format(name))
            if (p._start(handle, argc, argv) < 0):
                clicon_err(OE_FATAL, 0,
                           "{:s}.plugin_start() failed\n".format(name))
                return -1
            
    return 0

def _plugin_exit(h):
    global __plugins__

    handle = Handle(h)
    for name, p in __plugins__.items():
        if p._exit is not None:
            clicon_debug(1, "Calling {:s}.plugin_start()\n".format(name))
            p._start(handle, argc, argv)
            
    return 0
        
