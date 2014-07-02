/*
 *  CVS Version: $Id: clicon_spec.h,v 1.15 2013/09/20 11:45:08 olof Exp $
 *
  Copyright (C) 2009-2014 Olof Hagsand and Benny Holmgren
  Olof Hagsand
 *
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

 *
 * Database specification. This is the PT (parsetree) variant that corresponds to the keys
 * in the embedded database.
 */

#ifndef _CLICON_DBSPEC_PARSETREE_H_
#define _CLICON_DBSPEC_PARSETREE_H_

/*
 * Macros
 */

/*
 * Types
 */

/* Extract and copy of cligen necessary files for dbcli parser to work */
#include "clicon_dbspec_xxx.h"

/*
 * Prototypes
 */
int dbclispec_parse(clicon_handle h, const char *filename, dbspec_tree *pt);
struct dbspec_obj *key2spec_co(dbspec_tree *pt, char *key);
int dbspec_key2cli(clicon_handle h, struct db_spec *db_spec, dbspec_tree *pt);
struct db_spec *dbspec_cli2key(dbspec_tree *pt);
char *dbspec_indexvar_get(dbspec_obj *co);
int dbspec_indexvar_set(dbspec_obj *co, char *val);
char *dbspec_key_get(dbspec_obj *co);
int dbspec_key_set(dbspec_obj *co, char *val);
int dbspec_optional_get(dbspec_obj *co);
int dbspec_optional_set(dbspec_obj *co, int val);
struct cg_var *dbspec_default_get(dbspec_obj *co);
int dbspec_default_set(dbspec_obj *co, struct cg_var *val);
char dbspec_vector_get(dbspec_obj *co);
int dbspec_vector_set(dbspec_obj *co, char val);
int dbspec_userdata_add(clicon_handle h, dbspec_obj *co);
int dbspec_userdata_delete(dbspec_obj *co, void *arg);
int dbspec2dtd(FILE *f, dbspec_tree *pt);

#endif  /* _CLICON_DBSPEC_PARSETREE_H_ */
