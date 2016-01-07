/*

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

 */

#ifndef __CLICON_DBVARS_H__
#define __CLICON_DBVARS_H__

/*
 * Types
 */

typedef int (clicon_dbvalcb_t)(void *cbarg, cvec *vars, cg_var *var, char *func, cg_var *arg);

/* Return structure from clicon_dbvars_parse*() */
typedef struct {
    char		*dbv_key;	/* Database key */
    cvec		*dbv_vec;	/* Variable list */
} clicon_dbvars_t;

/* Internal data structure passed through yacc for dbvars parser */
typedef struct {
    clicon_dbvars_t	*dvp_ret;	/* Return struct */
    clicon_dbvalcb_t	*dvp_valcb;	/* */
    char		*dvp_db;	/* Database name for sequence checks */
    int			 dvp_varidx;	/* For unspec vars, use sequential */
    cvec		*dvp_vars;	/* cligen vars */
    void		*dvp_arg;	/* User specific args to cb */
} clicon_dbvarsparse_t;


/*
 * Macros
 */
/* typecast macro */
#define _YA ((clicon_dbvarsparse_t *)_ya)

/*
 * Prototypes
 */
void clicon_dbvars_free(clicon_dbvars_t *dvp);

clicon_dbvars_t *
clicon_dbvars_parse(dbspec_key *spec, 
		   char *db, 
		   cvec *vars,
		   const char *fmt,
		   clicon_dbvalcb_t *cb,
		   void *cb_arg);	/* user arg passed back to callback */

int clicon_dbvarslex(void *);
int clicon_dbvarsparse(void *);
void clicon_dbvarserror(void *_ya, char *);
int clicon_dbvars_debug(int d);
clicon_dbvars_t *clicon_set_parse(dbspec_key *spec, char *db, cvec *vars, const char *fmt);

#endif /* __CLICON_DBVARS_H__ */
