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
 * Database specification. This is the KEY variant that corresponds to the keys in the
 * embedded database.
 */

#ifndef _CLICON_SPEC_H_
#define _CLICON_SPEC_H_

/*
 * Macros
 */
/* Returns var-list of dbspec entry. Note, ds can be single entry and whole list */
#define db_spec2cvec(ds) (cvec*)((ds)->ds_vec)

/*
 * Types
 */
struct db_spec{
    struct db_spec *ds_next;   /* Linked list */
    char           *ds_key;    /* Keyword. eg foo[].bar */
    cvec           *ds_vec;    /* List of variables */
    int             ds_vector; /* allow list of variables to have same names, but
				not same values XXX but $a[] should be in ds_vec?*/
};

/*
 * Prototypes
 */
struct db_spec *db_spec_new(void); // static?
int db_spec_tailadd(struct db_spec **db_sp0, struct db_spec *db_s);// static?
struct db_spec *db_spec_parse_file(const char *filename);
int db_spec_free(struct db_spec *db_spec);
int db_spec_free1(struct db_spec *ds);
char *db_spec2str(struct db_spec *db);
int db_spec_dump(FILE *f, struct db_spec *db_spec);

#define db_spec_match(dbs, key) key2spec_key((dbs), (key)) /* obsolete */

struct db_spec *key2spec_key(struct db_spec *db_speclist, char *key);
int match_key(char *key, char *skey);
int sanity_check_cvec(char *key, struct db_spec *db_spec, cvec *vec);


#endif  /* _CLICON_SPEC_H_ */
