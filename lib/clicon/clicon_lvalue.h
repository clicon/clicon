/*
 *  CVS Version: $Id: clicon_lvalue.h,v 1.19 2013/08/01 09:15:46 olof Exp $
 *
  Copyright (C) 2009-2013 Olof Hagsand and Benny Holmgren
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

#ifndef _CLICON_LVALUE_H_
#define _CLICON_LVALUE_H_

#define V_UNIQUE	0x01	/* Variable flag */
#define V_WILDCARD	0x02	/* Variable flag */
#define V_SEQ		0x04	/* Sequence variable */
#define V_UNSET		0x08	/* Variable is unset */

/*
 * Types
 */
struct lvalue {
    uint16_t   lv_type;
    uint16_t   lv_len; /* Length of value */
    char       lv_val[0];
};

/* Help type for tree operation */
enum lv_op {LV_DELETE, LV_SET, LV_MERGE};
typedef enum lv_op lv_op_t;


/*
 * Variables
 */
int (*_clicon_lv_op_cb)(cg_var *cgv, char *varname, cvec *vr);

/*
 * Prototypes
 */ 
#define key_isanyvector(x)	(strstr((x), "[]") ? 1 : 0)
int key_isplain(char *key);
int key_isvector(char *key);
int key_isregex(char *key);
int key_iscomment(char *key);
int key_isvector_n(char *key);
int key_iskeycontent(char *key);
int key_isregex(char *key);

int keyindex_max_get(char *dbname, char *key, int *index, int regex);

struct lvalue *lv_element(char *val, int vlen, int n);

enum cv_type lv_arg_2_cv_type(char lv_arg);

int lv_dump(FILE *f, char *val, int vlen);

int lv_next_seq(char *dbname, char *basekey, char *varname, int increment);


char *db_lv_op_keyfmt (struct db_spec *dbspec,
		       char *dbname, 
		       char *basekey,
		       cvec *cvec,
		       const char *label);

int db_lv_op(struct db_spec *dbspec, char *dbname, lv_op_t op, 
	     char *fmt, cvec *vr);

int db_lv_op_exec(struct db_spec *dbspec, char *dbname, char *basekey, lv_op_t op, 
		  cvec *vh);

#if 0
int db_lv_op_lvec(struct db_spec *dbspec,
		  char *dbname, char *fmt, 
		  cvec *vr,
		  char **basekey, struct var_head **vh, const char *label);
#endif

char *db_lv_string(char *dbname, char *key, char *fmt);

int lv_matchvar (cvec *vhead1, cvec *vhead2, int cmpall);

char *cgv_fmt_string(cvec *vr, char *fmt);

int db_lv_set(struct db_spec *dbspec, char *dbname, 
	      char *key,  cvec *vh, lv_op_t op);
int
db_lv_vec_replace(char *dbname, char *basekey, 
		  char *prevval, char *newval);

int db_lv_vec_find(struct db_spec *dbspec, char *dbname, 
		   char *basekey, struct cvec *setvars, int *matched);
int
db_lv_spec2dbvec(struct db_spec *dbspec, 
		 char *dbname, char *veckey, int idx, cvec *vhead);

#endif  /* _CLICON_LVALUE_H_ */
