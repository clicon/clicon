/*
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


#include <Python.h>

/*
 * Python 2/3 support function: Convert a char* to python string
 */
PyObject *
StringFromString(const char *str)
{
#if PY_MAJOR_VERSION >= 3
    return PyUnicode_FromString(str);
#else
    return PyString_FromString(str);
#endif
}


/*
 * Python 2/3 support function: Convert a python string into a malloc:ed char*
 */
char *
StringAsString(PyObject *obj)
{
    char *str;
    char *retval = NULL;
    PyObject *strobj = NULL;
    

#if  PY_MAJOR_VERSION >= 3
    strobj = PyUnicode_AsUTF8String(obj);
    if (strobj == NULL)
	goto done; 
    str = PyBytes_AsString(strobj);
#else
    str = PyString_AsString(obj);
#endif
    if (str == NULL)
	goto done;

    retval = strdup(str);
done:
#if  PY_MAJOR_VERSION >= 3
    Py_XDECREF(strobj);
#endif
    
    return retval;	
}

/*
 * Python 2/3 support function: Convert a int to python int
 */
PyObject *
IntFromLong(long n)
{
#if PY_MAJOR_VERSION >= 3
    return PyLong_FromLong(n);
#else
    return PyInt_FromLong(n);
#endif
}

/*
 * Python 2/3 support function: Convert a int to python int
 */
long
IntAsLong(PyObject *n)
{
#if PY_MAJOR_VERSION >= 3
    return PyLong_AsLong(n);
#else
    return PyInt_AsLong(n);
#endif
}


/*
 * Python 2/3 support function: Get PyErr message as malloc:ed char*
 */
char *
ErrString(int restore)
{
    char *str = NULL;
    PyObject *type;
    PyObject *value;
    PyObject *traceback;
    
    if (!PyErr_Occurred())
	return NULL;
    
    PyErr_Fetch(&type, &value, &traceback);
    if (value) 
	str = StringAsString(value);
    
    if (restore)
	PyErr_Restore(type, value, traceback);
    else {
	Py_XDECREF(type);
	Py_XDECREF(value);
	Py_XDECREF(traceback);
    }

    return str;
}


/*
 * Python 2/3 support function: Get PyErr message as malloc:ed char*
 */
char *
ErrStringVerbose(int restore)
{
    char *msg = NULL;
    PyObject *type = NULL;
    PyObject *value = NULL;
    PyObject *traceback = NULL;
    PyObject *traceback_mod = NULL;
    char *file;
    int lineno;
    char *func;
    char *str;
    char *ret;
    size_t last;

    if (!PyErr_Occurred())
	return NULL;

    msg = ErrString(1);
    if (msg == NULL)
	return NULL;
    
    PyErr_Fetch(&type,&value,&traceback);
    PyErr_NormalizeException(&type,&value,&traceback);

    if (traceback == NULL)
	return msg;

    traceback_mod = PyImport_ImportModule("traceback");
    value =  PyObject_CallMethod(traceback_mod, "extract_tb", "O", traceback);
    if (value == NULL)
	PyErr_Print();
    
    last = PyList_Size(value)-1;
    file  = StringAsString(PyTuple_GetItem(PyList_GetItem(value, last), 0));
    lineno = IntAsLong(PyTuple_GetItem(PyList_GetItem(value, last), 1));
    func  = StringAsString(PyTuple_GetItem(PyList_GetItem(value, last), 2)); 
    str  = StringAsString(PyTuple_GetItem(PyList_GetItem(value, last), 3));
    
    ret = malloc(strlen(msg)+strlen(file)+strlen(func)+strlen(str)+10);
    sprintf(ret, "%s:%d->%s(): \"%s\": %s\n", file, lineno, func, str, msg);

    if (restore)
	PyErr_Restore(type, value, traceback);
    else {
	Py_XDECREF(type);
	Py_XDECREF(value);
	Py_XDECREF(traceback);
    }

    if (msg) free(msg);
    if (str) free(str);
    if (file) free(file);
    if (func) free(func);

    return ret;
}
