/*
 *  CVS Version: $Id: config_dbdiff.h,v 1.6 2013/08/01 09:15:46 olof Exp $
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
 * Code for comparing databases
 */

#ifndef _CONFIG_DBDIFF_H_
#define _CONFIG_DBDIFF_H_

/*
 * Types
 */ 
enum dbdiff_op{
  DBDIFF_OP_FIRST = 0x01,	
  DBDIFF_OP_SECOND = 0x02,
  DBDIFF_OP_BOTH = (DBDIFF_OP_FIRST|DBDIFF_OP_SECOND)
};

/*! Differences between two databases in terms of add, delete change per database key.
 *
 * Each entity contains two keys and an operation. There are two keys because the vector (.n) 
 * index can be different in the two databases. In most CLICON usages, the first db is 
 * typically running and the second is candidate, but the function is more general.
 *
 * The operation is one of:
 *  FIRST  - the key exists only in database 1, ie delete (removed in 2nd)
 *  SECOND - the key exists only in database 2, ie add (added in 2nd)
 *  BOTH   - the key exists in both databases, ie changed
 */
struct dbdiff_ent {
    char            *dfe_key1;	/* lvmap basekey (FIRST/BOTH) */
    char            *dfe_key2;	/* lvmap basekey (SECOND/BOTH) */
    enum dbdiff_op   dfe_op;	/* added, removed or changed */
};

/*! Top-level difference list between two databases.
 *
 * Given two databases, typically running and candidate, the dbdiff structure 
 * contains a list of entities (dbdiff_ent), each stating a database symbol and what the
 * difference is between the two. This top-level struct just contains the list of entities.
 */
struct dbdiff {
    int                  df_nr;	  /* Number of diffs */
    struct dbdiff_ent   *df_ents;   /* Vector of diff entries */
};

/*
 * Prototypes
 */ 
int db_diff(char *db1,     char *db2, 
	    const char *label,
	    struct db_spec *db_spec,
	    struct dbdiff *df
    );


#endif  /* _CONFIG_DBDIFF_H_ */
