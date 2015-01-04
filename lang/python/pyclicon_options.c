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
Clicon_options(PyObject *self, PyObject *args)
{
    int i;
    char **keys = NULL;
    size_t klen;
    PyObject *key;
    PyObject *options = NULL;
    clicon_hash_t *copt;
    clicon_handle h;

    if (!PyArg_ParseTuple(args, "O&", Clicon_unpack_handle, &h))
	return NULL;
    copt = clicon_options(h);
    if ((keys = hash_keys(copt, &klen)) == NULL)
	return PyErr_NoMemory();
    if ((options = PyTuple_New(klen)) == NULL)
	goto quit;
    for (i = 0; i < klen; i++) {
	if ((key = StringFromString(keys[i])) == NULL)
	    goto quit;
	PyTuple_SET_ITEM(options, i, key);
    }
    free(keys);

    return options;
    
quit:
    if (keys)
	free(keys);
    Py_XDECREF(options);
    
    return NULL;
}

PyObject *
Clicon_option(PyObject *self, PyObject *args)
{
    char *key;
    clicon_handle h;
    char *op;
    PyObject *str;

    if (!PyArg_ParseTuple(args, "O&s", Clicon_unpack_handle, &h, &key))
	return NULL;

    if ( ! clicon_option_exists(h, key))
	Py_RETURN_NONE;
    
    op = clicon_option_str(h, key);
    str = StringFromString(op);
    return str;
}

