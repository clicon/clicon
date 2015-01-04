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

 * Database specification parser cli syntax
 * (Cloned from cligen parser)
 */
#ifndef _CLICON_DBSPEC_H_
#define _CLICON_DBSPEC_H_

/*
 * Types
 */

/*
 * Two data structures: a list and a stack.
 * The stack keeps track of syntactic levels: (decl) and [decl] so that
 * the stack is pushed and later popped  to keep track of where you are.
 * The list keeps track of the list of objects that are currently affected. Every choice and 
 * option extends the list. Operations apply to all objects on the list..
 */
struct dbs_list{
    struct dbs_list *cl_next;
    dbspec_obj *cl_obj;
};

struct dbs_stack{
    struct dbs_stack *cs_next;
    struct dbs_list *cs_list;  /* Pointer to a list (saved state)*/
    struct dbs_list *cs_saved; /* Saved state for options (used in pop_add) */

};


struct clicon_dbspec_yacc_arg{
    clicon_handle         ya_handle;       /* cligen_handle */
    char                 *ya_name;         /* Name of syntax (for error string) */
    int                   ya_linenum;      /* Number of \n in parsed buffer */
    char                 *ya_parse_string; /* original (copy of) parse string */
    void                 *ya_lexbuf;       /* internal parse buffer from lex */
    cvec                 *ya_globals;     /* global variables after parsing */
    cvec                 *ya_cvec;     /* local variables (per-command) */
    struct dbs_stack     *ya_stack;     /* Stack of levels: push/pop on () and [] */
    struct dbs_list      *ya_list;      /* (Parallel) List of objects currently 'active' */
    dbspec_obj               *ya_var;
    int                   ya_lex_state;  /* lex start condition (ESCAPE/COMMENT) */
    int                   ya_lex_string_state; /* lex start condition (STRING) */

};

/* This is a malloced piece of code we attach to cligen objects used as db-specs.
 * So if(when) we translate cg_obj to dbspec_obj (or something). These are the fields
 * we should add.
 */
struct dbspec_userdata{
    char             *du_indexvar;  /* (clicon) This command is a list and
				       this string is the key/index of the list 
				    */
    char             *du_dbspec;    /* (clicon) Save dbspec key for cli 
				       generation */
    int               du_optional; /* (clicon) Optional element in list */
    struct cg_var    *du_default;   /* default value(clicon) */
    char              du_vector;    /* (clicon) Possibly more than one element */
};

/*
 * Variables
 */
extern char *clicon_dbspectext;

/*
 * Prototypes
 */


int dbspec_scan_init(struct clicon_dbspec_yacc_arg *ya);
int dbspec_scan_exit(struct clicon_dbspec_yacc_arg *ya);

int dbspec_parse_init(struct clicon_dbspec_yacc_arg *ya, dbspec_obj *co_top);
int dbspec_parse_exit(struct clicon_dbspec_yacc_arg *ya);

int clicon_dbspeclex(void *_ya);
int clicon_dbspecparse(void *);
void clicon_dbspecerror(void *_ya, char*);
int clicon_dbspec_debug(int d);

#endif	/* _CLICON_DBSPEC_H_ */
