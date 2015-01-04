/*
 *  CVS Version: $Id: linux-tunnel.c,v 1.2 2012/01/08 04:55:23 benny Exp $
 *
 *  Copyright (C) 2009-2015 Olof Hagsand and Benny Holmgren
 *
 *  This file is part of CLICON.
 *
 *  CLICON is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  CLICON is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CLICON; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_PYCLICON_CONFIG_H
#include "pyclicon_config.h" /* generated by config & autoconf */
#endif

#include <Python.h>

/* clicon */
#include <cligen/cligen.h>
#include <clicon/clicon.h>
#include <clicon/clicon_cli.h>

/* libpyclicon */
#include "libpyclicon.h"

static PyMethodDef cli_module_methods[] = {
#if 0
    {"_dbdep", (PyCFunction)_backend_dbdep, METH_VARARGS,
     "Register database dependency"
    },
#endif

    {NULL, NULL, 0, NULL}
};


PyMODINIT_FUNC
init__cli(void)
{
    PyObject* m;

    MOD_DEF(m,"_cliconcli","Python CLICON CLI frontend", cli_module_methods);
    if (m == NULL)
        return MOD_ERROR_VAL;
    
    return MOD_SUCCESS_VAL(m);
}

/*
 * plugin call
 */
static int
plugin_call(clicon_handle h, const char *func)
{
    int retval = -1;
    PyObject *handle = NULL;
    PyObject *cli = NULL;
    PyObject *val = NULL;
    
    if (Py_IsInitialized() == 0)
	return 0;

    PyErr_Clear();
    if ((cli = PyImport_ImportModule("cliconcli")) == NULL)
	goto quit;
    
    if ((handle = PyCapsule_New((void *)h, NULL, NULL)) == NULL)
	goto quit;
    
    val = PyObject_CallMethod(cli, func, "O", handle);
    if (val == NULL)
	goto quit;
    
    retval = PyLong_AsLong(val);

quit:
    if (PyErr_Occurred())
	clicon_err(OE_PLUGIN, 0, "%s", ErrStringVerbose(0));
    Py_XDECREF(cli);
    Py_XDECREF(handle);
    Py_XDECREF(val);

    return retval;
}



/*
 * Plugin initialization
 */
int
plugin_init(clicon_handle h)

{
    char *dir;
    int retval = -1;

    PyImport_AppendInittab("_cliconcli", init__cli);
    Py_InitializeEx(0);

    /* Append application plugin directory */
    if ((dir = clicon_option_str(h, "CLICON_CLI_DIR")) == NULL) {
	clicon_err(OE_PLUGIN, 0, "CLICON_CLI_DIR not set");
	goto quit;
    }
    if (syspath_insert(dir) < 0)
	goto quit;

    /* Append CLICON python module directory */
    if (syspath_insert(PYCLICON_MODDIR) < 0)
	goto quit;
    
    retval = plugin_call(h, "_plugin_init");

    
 quit:
    if (retval <= 0)  /* Error or no loaded plugins */
	Py_Finalize();

    if (debug && retval == 0)
	clicon_debug(1, "DEBUG: No python plugins loaded");
    else if (retval > 0)
	clicon_debug(1, "DEBUG: Loaded %d python plugins", retval);

    return (retval < 0) ? -1 : 0;
}

