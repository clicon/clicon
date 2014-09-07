/*
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
#ifdef HAVE_CONFIG_H
#include "clicon_config.h"
#endif


#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cligen/cligen.h>

/* clicon */
#include "clicon_err.h"
#include "clicon_queue.h"
#include "clicon_chunk.h"
#include "clicon_hash.h"
#include "clicon_db.h"
#include "clicon_handle.h"
#include "clicon_dbspec_key.h"
#include "clicon_lvalue.h"
#include "clicon_yang.h"
#include "clicon_dbutil.h"



/*! Search for key in database and return a vector of cligen variables.
 *
 * Find a key in the db and return its vector of values as a vector of cligen
 * variables.
 * Note: if key not found an empty cvec will be returned (not NULL).
 * Note: Returned cvec needs to be freed with cvec_free().
 *
 * @param   db     Name of database to search in (filename including dir path)
 * @param   key    String containing key to look for.
 *
 * @retval cv  A cligen vector containing all variables found. This vector contains no 
 *             variables (length == 0) if key is not found.
 */
cvec *
clicon_dbget(char *db, char *key)
{
    size_t           len;
    char            *lvec = NULL;
    cvec            *cvec = NULL;

    /* Dont expand vector indexes, eg A.n in a A[] entry */
    if(key_isvector_n(key) || key_iskeycontent(key))
	goto catch;
    /* Read key entry from database */
    if (db_get_alloc(db, key, (void*)&lvec, &len) < 0)
	goto catch;
    cvec = lvec2cvec(lvec, len);
catch:
    if(lvec)
	free(lvec);
    return cvec;
}

/*! Find a variable in the db
 *
 * Find a variable in the db given its name, a db-name and a key and return 
 * a single value or NULL if not found. If key contains many variable with the
 * same name, the 'first' is returned.
 * This is malloced so it must be freed (cv_free) by caller.
 */
cg_var *
clicon_dbgetvar(char *db, char *key, char *variable)
{
    cg_var             *cv = NULL;
    cg_var             *cv0;
    cvec               *cvec = NULL;

    /* First read cv vector from database */
    if ((cvec = clicon_dbget(db, key)) == NULL)
	goto catch;
    /* Pick up (first) variable from vector */
    if ((cv0 = cvec_find(cvec, variable)) == NULL)
	goto catch;
    if ((cv = cv_dup(cv0)) == NULL){
	clicon_err(OE_UNIX, errno, "cv_dup");
	goto catch;
    }
catch:
    if(cvec)
	cvec_free(cvec);
    return cv;
}



/*! Check if key exists database 
 *
 * Check if a named key is present in the database.
 *
 * @param   db     Name of database to search in (filename including dir path)
 * @param   key    Name of database key
 */
int
clicon_dbhaskey(char *db, char *key)
{
    return db_exists(db, key);
}

/*! Write cvec to database 
 *
 * Write a variable vector to database key.
 *
 * @param   db     Name of database to search in (filename including dir path)
 * @param   key    Name of database key
 * @param   vec    cliigen variable vector
 */
int
clicon_dbput(char *db, char *key, cvec *vec)
{
    char            *lvec = NULL;
    size_t           lvlen;
    
    /* Transform variable list to contiguous char vector */
    if ((lvec = cvec2lvec(vec, &lvlen)) == NULL)
	return -1;
    /* Write to database, key and a vector of variables */
    if (db_set(db, key, lvec, lvlen) < 0)
	return -1;
    return 0;
}

/*! Delete database key
 *
 * Delete named key from database
 *
 * @param   db     Name of database to search in (filename including dir path)
 * @param   key    Name of database key
 */
int
clicon_dbdel(char *db, char *key)
{
    return db_del(db, key);
}

/*! Delete variable from key
 *
 * Delete named variable from key in database
 *
 * @param   db       Name of database to search in (filename including dir path)
 * @param   key      Name of database key
 * @param   variable Name of variable to delete
 */
int
clicon_dbdelvar(char *db, char *key, char *variable)
{
    cvec *vec;
    cg_var *cv;

    if ((vec = clicon_dbget(db, key)) == NULL)
	return -1;
    
    if ((cv = cvec_find(vec, variable)) == NULL)
	return -1;

    cvec_del(vec, cv);
    return clicon_dbput(db, key, vec);
}

/*! Append varriable to db key
 *
 * Append a cligen variable to the named key in database
 *
 * @param   db       Name of database to search in (filename including dir path)
 * @param   key      Name of database key
 * @param   cv       Cligen variable
 */
int
clicon_dbappend(char *db, char *key, cg_var *cv)
{
    cvec *vec;
    cg_var *new;

    if ((vec = clicon_dbget(db, key)) == NULL)
	return -1;
    
    if ((new = cvec_add(vec, cv_type_get(cv))) == NULL) {
	clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
	return -1;
    }
    if (cv_cp(new, cv) < 0) {
	clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
	return -1;
    }

    return clicon_dbput(db, key, vec);
}


/*! Get list of db keys
 *
 * Get list of keys in database, excluding special vector keys.
 *
 * @param   db     Name of database to search in (filename including dir path)
 * @param   len    Pointer where length of returned list is stored.
 * @param   rx     Regular expression for key matching
 */
char **
clicon_dbkeys(char *db, size_t *len, char *rx)
{
    int i;
    int n;
    int keylen;
    int npairs;
    int nkeys;
    char *ptr;
    char **retval = NULL;
    char **keys = NULL;
    struct db_pair *pairs;

    if ((npairs = db_regexp(db, rx, __FUNCTION__, &pairs, 1)) < 0)
	return NULL;

    nkeys = 0;
    keylen = 0;
    for (i = 0; i < npairs; i++) {
	
	if(key_isvector_n(pairs[i].dp_key) || key_iskeycontent(pairs[i].dp_key))
	    continue;

	nkeys++;
	keylen += (strlen(pairs[i].dp_key)+1);
    }

    
    if ((keys = malloc(nkeys * sizeof(char *) + keylen)) == NULL) {
	clicon_err(OE_UNIX, errno, "%s: malloc", __FUNCTION__);
	goto quit;
    }
    
    n = 0;
    ptr = (char *)&keys[nkeys];
    for (i = 0; i < npairs; i++) {
	
	if(key_isvector_n(pairs[i].dp_key) || key_iskeycontent(pairs[i].dp_key))
	    continue;

	strcpy(ptr, pairs[i].dp_key);
	keys[n] = ptr;
	ptr += (strlen(pairs[i].dp_key)+1);
	n++;
    }

    *len = nkeys;
    retval = keys;

quit:
    unchunk_group(__FUNCTION__);
    if (retval == NULL)
	free(keys);
    
    return retval;
}



/*! Get list of db keys
 *
 * Get list of keys in database, excluding special vector keys.
 *
 * @param   db     Name of database to search in (filename including dir path)
 * @param   len    Pointer where length of returned list is stored.
 * @param   rx     Regular expression for key matching
 */
cvec **
clicon_dbitems(char *db, size_t *len, char *rx)
{
    int i;
    int n;
    int npairs;
    int nkeys;
    cvec **retval = NULL;
    cvec **items = NULL;
    struct db_pair *pairs;
    
    *len = 0;

    if ((npairs = db_regexp(db, rx, __FUNCTION__, &pairs, 0)) < 0)
	return NULL;
    
    nkeys = 0;
    for (i = 0; i < npairs; i++) {
	
	if(key_isvector_n(pairs[i].dp_key) || key_iskeycontent(pairs[i].dp_key))
	    continue;
	
	nkeys++;
    }

    if (nkeys == 0)
	return NULL;

    if ((items = calloc(nkeys, sizeof(cvec *))) == NULL) {
	clicon_err(OE_UNIX, errno, "%s: calloc", __FUNCTION__);
	goto quit;
    }
    
    n = 0;
    for (i = 0; i < npairs; i++) {
	
	if(key_isvector_n(pairs[i].dp_key) || key_iskeycontent(pairs[i].dp_key))
	    continue;

	if ((items[n] = lvec2cvec(pairs[i].dp_val, pairs[i].dp_vlen)) == NULL)
	    goto quit;
	if (cvec_name_set(items[n], pairs[i].dp_key) == NULL) {
	    clicon_err(OE_DB, 0, "%s: cvec_name_set", __FUNCTION__);
	    goto quit;
	}
	
	n++;
    }
    
    *len = nkeys;
    retval = items;

quit:
    unchunk_group(__FUNCTION__);
    if (retval == NULL) {
	if (items) {
	    for (i = 0; i < nkeys; i++)
		if (items[n])
		    cvec_free(items[n]);
	    free(items);
	}
    }
    
    return retval;
}
