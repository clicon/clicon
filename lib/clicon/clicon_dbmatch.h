/*
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

 * database utility functions. Here are functions not
 * directly related to lvmaps or lvalues.
 *                               
 */
#ifndef _CLICON_DBMATCH_H_
#define _CLICON_DBMATCH_H_

/*
 * Types
 */
/* Callback for dbmatch_fn() call 
 * returns 0 on success, -1 on error (and break), 1 on OK but break. */
typedef	int (*dbmatch_fn_t)(void *h, char *dbname, char *key, cvec *vr, void *arg);

/*
 * Prototypes
 */ 
int dbmatch_fn(void *handle, char *dbname, char *keypattern, char *attr, char *val, dbmatch_fn_t fn, void *arg, int *matches);

int dbmatch_vec(void *handle, char *dbname, char *keypattern, char *attr, 
		char *pattern, char ***keyp, cvec ***cvecp, int *lenp);

int dbmatch_vec_free(char **keyv, cvec **cvecv, int len);

int dbmatch_one(void *handle, char *dbname, char *keypattern, char *attr, char *pattern, char **keyp, cvec **cvecp);

char **dbvectorkeys(char *dbname, char *basekey, size_t *len);

int dbmatch_del(void *handle, char *dbname, char *keypattern, 
		char *attr, char *pattern, int *len);

#endif  /* _CLICON_DBMATCH_H_ */
