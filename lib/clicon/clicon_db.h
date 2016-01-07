/*
 *
  Copyright (C) 2009-2015 Olof Hagsand and Benny Holmgren
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

 */

#ifndef _CLICON_DB_H_
#define _CLICON_DB_H_

/*
 * General API
 */
cvec   *clicon_dbget(char *db, char *key);
cg_var *clicon_dbgetvar(char *db, char *key, char *variable);
int     clicon_dbput(char *db, char *key, cvec *vec);
int     clicon_dbputvar(char *db, char *key, cg_var *cv);
int     clicon_dbdel(char *db, char *key);
int     clicon_dbdelvar(char *db, char *key, char *variable);
int     clicon_dbappend(char *db, char *key, cg_var *cv);
int     clicon_dbexists(char *db, char *key);
int     clicon_dbmerge(char *db, char *key, cvec *vec);
int     clicon_dbkeys(char *db, char *rx, char ***keysv, size_t *len);
int     clicon_dbitems(char *db, char *rx, cvec ***cvv, size_t *len);
int     clicon_dbitems_match(char *db, char *rx, char *attr, char *pattern, cvec ***cvvp, size_t *lenp);
void    clicon_dbitems_free(cvec **vecs, size_t len);
int     clicon_dbget_parent(char *db, char *key, cvec **cvv);
int     clicon_dbget_children(char *db, char *key, cvec ***cvv, size_t *len);
int     clicon_dbget_descendants(char *db, char *key, cvec ***cvv, size_t *len);
int     clicon_dbget_xpath(clicon_handle h, char *dbname, cvec *cn, 
		       char *xpath, cvec ***cn_list, int *cn_len);


#endif  /* _CLICON_DB_H_ */
