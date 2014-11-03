/*
 *
  Copyright (C) 2009-2014 Olof Hagsand and Benny Holmgren

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

/*
 * Prototypes
 */ 
int cvec_del(cvec *vec, cg_var *cv);

cg_var *cvec_add_cv(cvec *vec, cg_var *cv);

cg_var *cvec_add_name(cvec *vec, enum cv_type type, char *name);

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

char *dbspec_unique_str(dbspec_key *ds, cvec *setvars);

char *dbspec_last_unique_str(dbspec_key *ds, cvec *setvars);

int cli_proto_change_cvec(clicon_handle h, char *db, lv_op_t op,
			  char *key, cvec *cvv);

#endif  /* _CLICON_DBUTIL_H_ */
