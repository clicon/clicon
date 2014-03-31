/*
 *  CVS Version: $Id: clicon_dbutil.h,v 1.17 2013/09/20 11:45:18 olof Exp $
 *
  Copyright (C) 2009-2013 Olof Hagsand and Benny Holmgren

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
#ifndef _CLICON_DBUTIL_H_
#define _CLICON_DBUTIL_H_

/*
 * Types
 */
/* Callback for dbmatch() call 
 * returns 0 on success, -1 on error (and break), 1 on OK but break. */
typedef	int (*dbmatch_fn_t)(void *h, char *dbname, char *key, cvec *vr, void *arg);

/*
 * Prototypes
 */ 
int cvec_del(cvec *vec, cg_var *cv);

cg_var *cvec_add_cv(cvec *vec, cg_var *cv);

int cvec_merge(cvec *orig, cvec *add);

int cvec_merge2(cvec *orig, cvec *add);

cg_var *dbvar2cv(char *dbname, char *key, char *variable);

cvec *dbkey2cvec(char *dbname, char *key);

int cvec2dbkey(char *dbname, char *key, cvec *cvec);

cvec *lvec2cvec(char *lvec, size_t lveclen);

char *cvec2lvec(cvec *vr, size_t *len);

int lv2cv(struct lvalue *lv, cg_var *cgv);

struct lvalue *cv2lv (cg_var *cgv);

char *lv2str(struct lvalue *lv);

char *db_gen_rxkey(char *basekey, const char *label);

char *db_match_key_byattr(char *dbname, char *keyspec, char *attr, char *name);

char *dbspec_last_unique_str(struct db_spec *ds, cvec *setvars);

int dbmatch(void *handle, char *dbname, char *keypattern, char *attr, char *val, dbmatch_fn_t fn, void *arg, int *matches);

int dbmatch_vec(void *handle, char *dbname, char *keypattern, char *attr, 
		char *pattern, char ***keyp, cvec ***cvecp, int *lenp);

int dbmatch_vec_free(char **keyv, cvec **cvecv, int len);

int dbfind(void *handle, char *dbname, char *keypattern, char *attr, char *pattern, char **keyp, cvec **cvecp);

char **dbvectorkeys(char *dbname, char *basekey, size_t *len);

int cli_proto_change_cvec(clicon_handle h, char *db, lv_op_t op,
			  char *key, cvec *cvv);

#endif  /* _CLICON_DBUTIL_H_ */
