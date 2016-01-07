/*
 *  CVS Version: $Id: linux-tunnel.c,v 1.2 2012/01/08 04:55:23 benny Exp $
 *
  Copyright (C) 2009-2015 Olof Hagsand and Benny Holmgren

  This file is part of CLICON.

  CLICON is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  CLICON is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with CLICON; see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>.
 */

#include <Python.h>

/* Avoid 'dereferencing type-punned pointer will break strict-aliasing rules'
 * warnings from Py_RETURN_TRUE and Py_RETURN_FALSE.
 */
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

/* clicon */
#include <cligen/cligen.h>
#include <clicon/clicon.h>

/* pyclicon */
#include "pyclicon.h"
#include "py23.h"


typedef struct _CliconDB {
    PyObject_HEAD
    char *filename;
} CliconDB;



static void
CliconDB_dealloc(CliconDB *self)
{
    free(self->filename);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
CliconDB_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return (PyObject *)type->tp_alloc(type, 0);
}

static int
CliconDB_init(CliconDB *self, PyObject *args, PyObject *kwds)
{
    char *filename;

    if (!PyArg_ParseTuple(args, "s", &filename))
        return -1;

    if ((self->filename = strdup(filename)) == NULL)
	return -1;

    return 0;
}

static PyObject *
_db_init(CliconDB *self)
{
    if (db_init(self->filename) < 0) {
	PyErr_Format(PyExc_RuntimeError, /* XXX Need CLICON exceptoions */
		     "Failed to initialize database '%s'", self->filename);
	return NULL;
    }
    
    Py_RETURN_TRUE;
}
    

static PyObject *
_db_getvar(CliconDB *self, PyObject *args)
{
    char *key;
    char *var;
    char *valstr;
    cg_var *cv;
    PyObject *Cv;

    if (!PyArg_ParseTuple(args, "ss", &key, &var))
        return NULL;

    if ((cv = clicon_dbgetvar(self->filename, key, var)) == NULL)
	Py_RETURN_NONE;

    if ((valstr = cv2str_dup(cv)) == NULL) {
        PyErr_SetString(PyExc_MemoryError, "failed to allocate memory");
	cv_free(cv);
        return NULL;
    }
    Cv = PyObject_CallMethod(__cligen_module(), "CgVar", "iss",
			     cv_type_get(cv), cv_name_get(cv), valstr);
    free(valstr);
    cv_free(cv);

    return Cv;
}

static PyObject *
_db_get(CliconDB *self, PyObject *args)
{
    char *key;
    char *valstr;
    cvec *vr = NULL;
    PyObject *Cv;
    PyObject *Val;
    PyObject *Cvec = NULL;
    PyObject *retval = NULL;
    cg_var *cv;

    if (!PyArg_ParseTuple(args, "s", &key))
        return NULL;

    if ((vr = clicon_dbget(self->filename, key)) == NULL || cvec_len(vr) == 0)
	Py_RETURN_NONE;

    if ((Cvec = PyObject_CallMethod(__cligen_module(), "Cvec", NULL)) == NULL)
	goto done;
    
    for (cv = NULL; (cv = cvec_each(vr, cv)); ) {
	if ((valstr = cv2str_dup(cv)) == NULL) {
	    PyErr_SetString(PyExc_MemoryError, "failed to allocate memory");
	    goto done;
	}
	Cv = PyObject_CallMethod(__cligen_module(), "CgVar", "iss",
				 cv_type_get(cv), cv_name_get(cv), valstr);
	free(valstr);
	if (Cv == NULL)
	    goto done;
	Val = PyObject_CallMethod(Cvec, "append", "O", Cv);
	Py_DECREF(Cv);
	if (Val == NULL)
	    goto done;
	Py_DECREF(Val);
    }
    retval = Cvec;

done:
    if (vr)
	cvec_free(vr);
    if (retval == NULL)
	Py_XDECREF(Cvec);
    
    return retval;
}

static PyObject *
_db_put(CliconDB *self, PyObject *args)
{
    char *key;
    cg_var *cv;
    cg_var *new;
    cvec *vr = NULL;
    PyObject *Cvec; 
    PyObject *Cv = NULL;
    PyObject *Iterator = NULL;
    PyObject *Item = NULL;
    
    if (!PyArg_ParseTuple(args, "sO", &key, &Cvec))
        return NULL;

    if ((Iterator = PyObject_GetIter(Cvec)) == NULL)
	return NULL;

    if ((vr = cvec_new(0)) == NULL) {
        PyErr_SetString(PyExc_MemoryError, NULL);
	goto quit;
    }
	
    while ((Item = PyIter_Next(Iterator))) {
	if ((Cv = PyObject_CallMethod(Item, "__cv", NULL)) == NULL)
	    goto quit;
	if ((cv = PyCapsule_GetPointer(Cv, NULL)) == NULL)
	    goto quit;
	if ((new = cvec_add(vr, cv_type_get(cv))) == NULL)
	    goto quit;
	if (cv_cp(new, cv) < 0) {
	    PyErr_SetString(PyExc_MemoryError, NULL);
	    goto quit;
	}
	Py_DECREF(Cv);
	Cv = NULL;
	Py_DECREF(Item);
	Item = NULL;
    }
    Py_DECREF(Iterator);

    
    if (clicon_dbput(self->filename, key, vr) < 0) {
	PyErr_Format(PyExc_RuntimeError, /* XXX Need CLICON exceptions */
		     "Failed to write data to database '%s'",
		     self->filename);
	goto quit;
    }
    cvec_free(vr);

    Py_RETURN_TRUE;
    
quit:
    if (vr)
	cvec_free(vr);
    Py_XDECREF(Iterator);
    Py_XDECREF(Item);
    Py_XDECREF(Cv);

    return NULL;
}

static PyObject *
_db_del(CliconDB *self, PyObject *args)
{
    int status;
    char *key;

    if (!PyArg_ParseTuple(args, "s", &key))
        return NULL;
    
    status = db_del(self->filename, key);
    
    if (status < 0) {
	/* XXX Need CLICON exceptoions */
	PyErr_Format(PyExc_RuntimeError, "CLICON: db_del() failed");
	return NULL;
    }

    if (status > 0)
	Py_RETURN_TRUE;
    else 
	Py_RETURN_FALSE;
}
	
static PyObject *
_keys(CliconDB *self, PyObject *args)
{
    int i;
    size_t nkeys;
    char **keys;
    PyObject *Keys;
    PyObject *Key;
    char *rx = ".*";

    if (!PyArg_ParseTuple(args, "|s", &rx))
        return NULL;
    
    if (clicon_dbkeys(self->filename, rx, &nkeys, &keys) < 0) {
	/* XXX Need CLICON exceptoions */
	PyErr_Format(PyExc_RuntimeError, "clicon_dbkeys failed");
	return NULL;
    }
    
    if ((Keys = PyTuple_New(nkeys)) == NULL) {
	free(keys);
	return NULL;
    }

    for (i = 0; i < nkeys; i++) {
	if ((Key = StringFromString( keys[i])) == NULL) {
	    Py_DECREF(Keys);
	    free(keys);
	    return NULL;
	}
	PyTuple_SET_ITEM(Keys, i, Key);
    }
    free(keys);

    return Keys;    
}

static PyObject *
_items(CliconDB *self, PyObject *args)
{
    int i;
    size_t len;
    cvec **items = NULL;
    PyObject *Ret;
    PyObject *Item = NULL;
    PyObject *Items = NULL;
    PyObject *Key = NULL;
    PyObject *Val = NULL;
    PyObject *Retval = NULL;
    PyObject *Capsule = NULL;
    char *rx = ".*";

    if (!PyArg_ParseTuple(args, "|s", &rx))
        return NULL;
    
    if (clicon_dbitems(self->filename, rx, &len, &items) < 0) {
	/* XXX Need CLICON exceptions */
	PyErr_Format(PyExc_RuntimeError, "clicon_dbitems failed");
	return NULL;
    }
    
    if ((Items = PyList_New(len)) == NULL)
	goto quit;
    
    for (i = 0; i < len; i++) {
	
	/* Create key object */
	if ((Key = StringFromString(cvec_name_get(items[i]))) == NULL)
	    goto quit;
	
	if ((Capsule = PyCapsule_New((void *)items[i], NULL, NULL)) == NULL)
	    goto quit;

	if ((Val = PyObject_CallMethod(__cligen_module(),"Cvec", NULL)) == NULL)
	    goto quit;
	if ((Ret = PyObject_CallMethod(Val, "__Cvec_from_cvec",
				       "O", Capsule)) == NULL)
	    goto quit;
	Py_DECREF(Ret);

	/* Create key/val pair Tuple */
	if ((Item = PyTuple_New(2)) == NULL)
	    goto quit;
	PyTuple_SET_ITEM(Item, 0, Key);
	PyTuple_SET_ITEM(Item, 1, Val);
	Key = Val = NULL; /* References stolen by Item */

	/* Append tuple to list */
	PyList_SET_ITEM(Items, i, Item);
	    goto quit;
	Item = NULL;
    }

    Retval = Items;

quit:
    if (Retval == NULL)
	Py_XDECREF(Items);
    if (items)
	clicon_dbitems_free(items, len);
    Py_XDECREF(Item);
    Py_XDECREF(Key);
    Py_XDECREF(Val);
    Py_XDECREF(Capsule);
    
    return Retval;
}


static PyObject *
_db2txt(CliconDB *self, PyObject *args)
{
    char *syntax;
    char *str;
    PyObject *retval;
    clicon_handle h;

    if (!PyArg_ParseTuple(args, "O&s", Clicon_unpack_handle, &h, &syntax))
	return NULL;

    if ((str = clicon_db2txt_buf(h, self->filename, syntax)) == NULL)
	Py_RETURN_NONE;
   
    retval = StringFromString(str);
    free(str);
    
    return retval;
}


/*
 * Load clicon python api
 */
static PyMethodDef CliconDB_methods[] = {

    {"_init", (PyCFunction)_db_init, METH_NOARGS,
     "Initialize database file"},
    {"_db_getvar", (PyCFunction)_db_getvar, METH_VARARGS,
     "Get variable value from database"},
    {"_db_get", (PyCFunction)_db_get, METH_VARARGS,
     "Get variable vector for a key from database"},
    {"_db_put", (PyCFunction)_db_put, METH_VARARGS,
     "Write an Cvec to db"},
    {"_db_del", (PyCFunction)_db_del, METH_VARARGS,
     "Delete a key from the database"},
    {"_keys",  (PyCFunction)_keys, METH_VARARGS,
     "Get contents from databased based on key regexp"},
    {"_items",  (PyCFunction)_items, METH_VARARGS,
     "Get list of key/value tuples from database"},
    {"_db2txt",  (PyCFunction)_db2txt, METH_VARARGS,
     "Generate text output based on text format and database contents"},

    {NULL, NULL, 0, NULL}
};

PyTypeObject CliconDB_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_clicon.CliconDB",        /* tp_name */
    sizeof(CliconDB),          /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)CliconDB_dealloc, /* tp_dealloc */
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
    "CLICON database object",  /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    CliconDB_methods,          /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)CliconDB_init,   /* tp_init */
    0,                         /* tp_alloc */
    CliconDB_new,              /* tp_new 0*/
};


int
CliconDB_init_object(PyObject *m)
{

    if (PyType_Ready(&CliconDB_Type) < 0)
        return -1;

    Py_INCREF(&CliconDB_Type);
    PyModule_AddObject(m, "_CliconDB", (PyObject *)&CliconDB_Type);

    return 0;
}

