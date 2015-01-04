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
Clicon_err(PyObject *self, PyObject *args)
{
    int line;
    int err;
    int suberr;
    char *func;
    char *msg;
    int ret;
    
    if (!PyArg_ParseTuple(args, "siiis", &func, &line, &err, &suberr, &msg))
	return NULL;

    ret = clicon_err_fn(func, line, err, suberr, msg);
    return IntFromLong(ret);
}


PyObject *
Clicon_strerror(PyObject *self, PyObject *args)
{
    int err;

    if (!PyArg_ParseTuple(args, "i", &err))
	return NULL;
    
    return StringFromString(clicon_strerror(err));
}

PyObject *
Clicon_err_reset(PyObject *self)
{
    clicon_err_reset();
    Py_RETURN_NONE;
}


int
Clicon_err_init(PyObject *m)
{
    PyModule_AddIntConstant(m, (char *) "OE_UNDEF", OE_UNDEF);
    PyModule_AddIntConstant(m, (char *) "OE_DB", OE_DB);
    PyModule_AddIntConstant(m, (char *) "OE_DEMON", OE_DEMON);
    PyModule_AddIntConstant(m, (char *) "OE_EVENTS", OE_EVENTS);
    PyModule_AddIntConstant(m, (char *) "OE_CFG", OE_CFG);
    PyModule_AddIntConstant(m, (char *) "OE_PROTO", OE_PROTO);
    PyModule_AddIntConstant(m, (char *) "OE_REGEX", OE_REGEX);
    PyModule_AddIntConstant(m, (char *) "OE_UNIX", OE_UNIX);
    PyModule_AddIntConstant(m, (char *) "OE_SYSLOG", OE_SYSLOG);
    PyModule_AddIntConstant(m, (char *) "OE_ROUTING", OE_ROUTING);
    PyModule_AddIntConstant(m, (char *) "OE_XML", OE_XML);
    PyModule_AddIntConstant(m, (char *) "OE_PLUGIN", OE_PLUGIN);
    PyModule_AddIntConstant(m, (char *) "OE_FATAL", OE_FATAL);

    return 0;
}

