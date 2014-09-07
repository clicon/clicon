/* 
 * pyclicon.h
 *
 * Copyright (C) 2009-2014 Olof Hagsand and Benny Holmgren
 *
 * This file is part of PyCLICON.
 *
 * PyCLICON is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 *  PyCLICON is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along wth PyCLICON; see the file LICENSE.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __PY_CLICON_H__
#define __PY_CLICON_H__

#include "pyclicon_err.h"
#include "pyclicon_log.h"
#include "pyclicon_options.h"


/*
 * Internal functions
 */
PyObject *__cligen_module(void);
int Clicon_unpack_handle(PyObject *handle, clicon_handle *h);


#endif /* __PY_CLICON_H__ */
