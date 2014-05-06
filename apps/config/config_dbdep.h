/*
 *  CVS Version: $Id: config_dbdep.h,v 1.13 2013/08/05 14:19:55 olof Exp $
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

 *
 */

#ifndef _CONFIG_DBDEP_H_
#define _CONFIG_DBDEP_H_

/*
 * Types
 */ 
/* Database dependency description entry */
struct dbdep_ent {
    qelem_t 	 dpe_qelem;	/* List header */
    char	*dpe_key;	/* Database key dependency */
    char	*dpe_var;	/* Regexp of variable, NULL=all */
};
typedef struct dbdep_ent dbdep_ent_t;


/* Database dependency description */
struct dbdep {
    qelem_t 	 dp_qelem;	/* List header */
    uint16_t	 dp_row;	/* "Row" number for processing order */
    enum trans_cb_type dp_type; /* Commit, validate or both */
    trans_cb	 dp_callback;	/* Validation/Commit Callback */
    void	*dp_arg;	/* Application specific argument to cb */
    dbdep_ent_t *dp_ent;	/* List of key/vars */
};
typedef struct dbdep dbdep_t;

/* Commit operation list */
struct dbdep_dd {
    dbdep_t		*dd_dep;
    struct dbdiff_ent	*dd_dbdiff;
};
typedef struct dbdep_dd dbdep_dd_t;


/*
 * Internal prototypes
 */ 
void dbdeps_free(clicon_handle h);

int dbdep_commitvec(clicon_handle h, const struct dbdiff *, int *, dbdep_dd_t **);

#endif  /* _CONFIG_DBDEP_H_ */
