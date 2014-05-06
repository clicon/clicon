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

/*
 * There are two keys because the vector index can be different in the
 * two databases.
 */
struct dbdiff_ent {
    char            *dfe_key1;	/* lvmap basekey (FIRST/BOTH) */
    char            *dfe_key2;	/* lvmap basekey (SECOND/BOTH) */
    enum dbdiff_op   dfe_op;	/* added, removed or changed */
};

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
