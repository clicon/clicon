/*
 *
 * Copyright (C) 2009-2014 Olof Hagsand and Benny Holmgren
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
#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <Python.h>

/* clicon */
#include <cligen/cligen.h>
#include <clicon/clicon.h>

/* pyclicon */
#include "pyclicon.h"


typedef struct _backend {
    PyObject_HEAD
} _backend;


static void
backend_dealloc(clicon *self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
backend_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return (PyObject *)type->tp_alloc(type, 0);
}

static int
backend_init(clicon *self, PyObject *args, PyObject *kwds)
{
    return 0;
}

/*
 * Load clicon python api
 */
static PyMethodDef backend_methods[] = {

#if 0
    {"dbdep", py_dbdep, METH_VARARGS,
     "Register configuration dependencies."},
    {"dbdep_ent", py_dbdep_ent, METH_VARARGS,
     "Register configuration dependency entry for existing dependency."},
#endif

    {NULL, NULL, 0, NULL}
};

PyTypeObject Backend_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_backend",                /* tp_name */
    sizeof(backend),           /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)backend_dealloc, /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,   /* tp_flags */
    "CLICON Backend object",   /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    backend_methods,           /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)backend_init,    /* tp_init */
    0,                         /* tp_alloc */
    backend_new,               /* tp_new */
};

static PyMethodDef backend_module_methods[] = {
#if 0
    {"_db2txt", (PyCFunction)CliconBackend_db2txt, METH_VARARGS,
     "Format text output based on the CLICON db2txt formatting language"
    },
#endif

    {NULL, NULL, 0, NULL}
};

MOD_INIT(_backend)
{
    PyObject* m;

    MOD_DEF(m,"_backend", "Python bindings for CLICON", backend_module_methods);
    if (m == NULL)
        return MOD_ERROR_VAL;
    
    if (PyType_Ready(&Backend_Type) < 0)
        return MOD_ERROR_VAL;

    Py_INCREF(&Backend_Type);
    PyModule_AddObject(m, "backend", (PyObject *)&Backend_Type);

    return MOD_SUCCESS_VAL(m);
}
