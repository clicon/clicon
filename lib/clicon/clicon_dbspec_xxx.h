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
 * This is an extract and copy of cligen necessary files for dbcli parser to work.
 * Plan is to replace this with yang parser.
 */

#ifndef _CLICON_DBSPEC_XXX_H_
#define _CLICON_DBSPEC_XXX_H_

/*
 * Types
 */
struct dbspec_tree{
    struct dbspec_obj     **dt_vec;    /* vector of pointers to parse-tree nodes */
    int                 dt_len;    /* length of vector */
};
typedef struct dbspec_tree dbspec_tree;

/*
 * If cligen object is a variable, this is its variable spec.
 * But it is not complete, it is a part of a cg_obj.
 * A cg_var is the value of such a varspec.
 */
struct cg_varspec2{
    enum cv_type    cgs_vtype;         /* its type */
    char           *cgs_expand_fn_str; /* expand callback string */
    expand_cb      *cgs_expand_fn;     /* expand callback */
    cg_var         *cgs_expand_fn_arg; /* expand callback arg */
    char           *cgs_choice;        /* list of choices */
    int             cgs_range;         /* range interval valid */
    int64_t         cgs_range_low;     /* range interval lower limit*/
    int64_t         cgs_range_high;    /* range interval upper limit*/
    char           *cgs_regex;         /* regular expression */
};
typedef struct cg_varspec2 cg_varspec2;


struct dbspec_obj{
    dbspec_tree        do_pt;        /* Child parse-tree (see co_next macro below) */
    struct dbspec_obj *do_prev;
    enum cg_objtype    do_type;      /* Type of object */
    char              *do_command;   /* malloc:ed matching string / name or type */

    char	      *do_help;	    /* Brief helptext */
    void              *do_userdata;  /* User-specific data, malloced and defined by
				       the user. Will be freed by cligen on exit */
    size_t             do_userlen;   /* Length of the userdata (need copying) */
    union {
	struct {        } dou_cmd;
	struct cg_varspec2 dou_var;
    } u;
};
typedef struct dbspec_obj dbspec_obj; /* in cli_var.h */

/* Access fields for code traversing parse tree */
#define do_next          do_pt.dt_vec
#define do_max           do_pt.dt_len
#define do_vtype         u.dou_var.cgs_vtype
#define do_choice	 u.dou_var.cgs_choice
#define do_keyword	 u.dou_var.cgs_choice
#define do_range	 u.dou_var.cgs_range
#define do_range_low	 u.dou_var.cgs_range_low
#define do_range_high	 u.dou_var.cgs_range_high
#define do_regex         u.dou_var.cgs_regex

#define iskeyword2(CV) ((CV)->do_choice!=NULL && strchr((CV)->do_choice, '|')==NULL)

#define do2varspec(co)  &(co)->u.dou_var



typedef int (cg_applyfn2_t)(dbspec_obj *co, void *arg);

/*
 * Prototypes
 */
static inline int
co_up_set2(dbspec_obj *co, dbspec_obj *cop) 
{
    co->do_prev = cop;
    return 0;
}

int         pt_apply2(dbspec_tree pt, cg_applyfn2_t fn, void *arg);
dbspec_obj *co_new2(char *cmd, dbspec_obj *prev);
//int        co_free2(dbspec_obj *co, int recursive);
int        cligen_parsetree2_free(dbspec_tree pt, int recursive);
dbspec_obj *co_insert2(dbspec_tree *pt, dbspec_obj *co1);
dbspec_obj *co_find_one2(dbspec_tree pt, char *name);
int         cv_validate2(cg_var *cv, cg_varspec2 *cs, char **reason);

#endif  /* _CLICON_DBSPEC_XXX_H_ */
