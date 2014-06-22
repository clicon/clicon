/*
 *  CVS Version: $Id: clicon_dbspec.h,v 1.2 2013/09/13 15:05:39 olof Exp $
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

 * Database specification parser cli syntax
 * (Cloned from cligen parser)
 */
#ifndef _CLICON_YANG_PARSE_H_
#define _CLICON_YANG_PARSE_H_

/*
 * Types
 */


struct clicon_yang_yacc_arg{
    clicon_handle         ya_handle;       /* cligen_handle */
    char                 *ya_name;         /* Name of syntax (for error string) */
    int                   ya_linenum;      /* Number of \n in parsed buffer */
    char                 *ya_parse_string; /* original (copy of) parse string */
    void                 *ya_lexbuf;       /* internal parse buffer from lex */
    cvec                 *ya_globals;     /* global variables after parsing */
    cvec                 *ya_cvec;     /* local variables (per-command) */
    struct dbs_stack     *ya_stack;     /* Stack of levels: push/pop on () and [] */
    struct dbs_list      *ya_list;      /* (Parallel) List of objects currently 'active' */
    cg_obj               *ya_var;
    int                   ya_lex_state;  /* lex start condition (ESCAPE/COMMENT) */
    int                   ya_lex_string_state; /* lex start condition (STRING) */

};

/* This is a malloced piece of code we attach to cligen objects used as db-specs.
 * So if(when) we translate cg_obj to yang_obj (or something). These are the fields
 * we should add.
 */
struct yang_userdata{
    char             *du_indexvar;  /* (clicon) This command is a list and
				       this string is the key/index of the list 
				    */
    char             *du_yang;    /* (clicon) Save yang key for cli 
				       generation */
    int               du_optional; /* (clicon) Optional element in list */
    struct cg_var    *du_default;   /* default value(clicon) */
    char              du_vector;    /* (clicon) Possibly more than one element */
};

/*
 * Variables
 */
extern char *clicon_yangtext;

/*
 * Prototypes
 */


int yang_scan_init(struct clicon_yang_yacc_arg *ya);
int yang_scan_exit(struct clicon_yang_yacc_arg *ya);

int yang_parse_init(struct clicon_yang_yacc_arg *ya, cg_obj *co_top);
int yang_parse_exit(struct clicon_yang_yacc_arg *ya);

int clicon_yanglex(void *_ya);
int clicon_yangparse(void *);
void clicon_yangerror(void *_ya, char*);
int clicon_yang_debug(int d);

#endif	/* _CLICON_YANG_PARSE_H_ */
