/*
 *
  Copyright (C) 2009-2015 Olof Hagsand and Benny Holmgren
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

#ifndef _CLICON_DBSPEC_KEY_H_
#define _CLICON_DBSPEC_KEY_H_

/*
 * Macros
 */
/* Returns var-list of dbspec entry. Note, ds can be single entry and whole list.
 * XXX: consider renaming, nothing to do with actual database.
 */
#define db_spec2cvec(ds) (cvec*)((ds)->ds_vec)

/*
 * Types
 */
struct dbspec_key{
    struct dbspec_key *ds_next;   /* Linked list */
    char              *ds_key;    /* Keyword. eg foo[].bar */
    cvec              *ds_vec;    /* List of variables */
    int                ds_vector; /* allow list of variables to have same names, but
				     not same values. Only set in yang2key_leaf_list() */
    /* Record keeping for head of list */
    struct dbspec_key *ds_tail;   /* Tail of linked list */
    clicon_hash_t     *ds_index;  /* Hashed index of keys in whole list */
};
typedef struct dbspec_key dbspec_key;

/*
 * Prototypes
 */
dbspec_key *db_spec_new(void); // static?
int db_spec_tailadd(dbspec_key **db_sp0, dbspec_key *db_s);// static?
int db_spec_free(dbspec_key *db_spec);
char *db_spec2str(dbspec_key *db);
int db_spec_dump(FILE *f, dbspec_key *db_spec);
dbspec_key *key2spec_key(dbspec_key *db_speclist, char *key);
int sanity_check_cvec(char *key, dbspec_key *db_spec, cvec *vec);
int dbspec_key_main(clicon_handle h, FILE *f, int printspec, int printalt);

#endif  /* _CLICON_DBSPEC_KEY_H_ */
