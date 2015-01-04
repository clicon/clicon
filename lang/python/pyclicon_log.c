/*
 * Copyright (C) 2009-2015 Olof Hagsand and Benny Holmgren
 *
 * This file is part of CLICON.
 * 
 * CLICON is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 *  CLICON is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CLICON; see the file COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <Python.h>

#include <cligen/cligen.h>
#include <clicon/clicon.h>

#include "pyclicon.h"
#include "py23.h"

PyObject *
Clicon_debug(PyObject *self, PyObject *args)
{
    int level;
    char *msg;
    
    if (!PyArg_ParseTuple(args, "is", &level, &msg))
	return NULL;

    return IntFromLong(clicon_debug(level, msg));
}


PyObject *
Clicon_log(PyObject *self, PyObject *args)
{
    int level;
    char *msg;
    
    if (!PyArg_ParseTuple(args, "is", &level, &msg))
	return NULL;

    return IntFromLong(clicon_log_str(level, msg));
}


int
Clicon_log_init(PyObject *m)
{
#if 0   /* Not needed unless we want binding for clicon_log_init() */
    PyModule_AddIntConstant(m, (char *)"CLICON_LOG_SYSLOG", CLICON_LOG_STYSLOG);
    PyModule_AddIntConstant(m, (char *)"CLICON_LOG_STDERR", CLICON_LOG_STDERR);
    PyModule_AddIntConstant(m, (char *)"CLICON_LOG_STDOUT", CLICON_LOG_STDOUT);
#endif

    return 0;
}

