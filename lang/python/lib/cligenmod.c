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

/* clicon */
#include <cligen/cligen.h>
#include <clicon/clicon.h>

#include "py23.h"

PyObject *
__cligen_module()
{
  static PyObject *cligen = NULL;

  if (!cligen)
    cligen = PyImport_ImportModule("cligen");

  return cligen;  
}
