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

#define align4(s) (((s)/4)*4 + 4) /* to avoid warnings in valgrind memory check */

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
    cvec_name_set(cvec, key);

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
clicon_dbexists(char *db, char *key)
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
    int              retval = -1;
    char            *lvec = NULL;
    size_t           lvlen;
    
    /* Transform variable list to contiguous char vector */
    if ((lvec = cvec2lvec(vec, &lvlen)) == NULL)
	goto done;
    /* Write to database, key and a vector of variables */
    if (db_set(db, key, lvec, lvlen) < 0)
	goto done;
    retval = 0;
 done:
    if (lvec)
	free(lvec);
    return retval;
}

/*! Set variable in database key
 *
 * Set named variable int database key. If database key doesn't exist it will
 * be created.
 *
 * @param   db       Name of database to search in (filename including dir path)
 * @param   key      Name of database key
 * @param   cv       Variable to set
 */
int
clicon_dbputvar(char *db, char *key, cg_var *cv)
{
    char *name;
    cvec *vec;
    cg_var *new = NULL;
    int retval = -1;

    if ((vec = clicon_dbget(db, key)) == NULL) {
	if ((vec = cvec_new(0)) == NULL) {
	    clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
	    goto quit;
	}
    }
    
    name = cv_name_get(cv);
    if (name == NULL || (new = cvec_find(vec, name)) == NULL) {
	if ((new = cvec_add(vec, cv_type_get(cv))) == NULL) {
	    clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
	    goto quit;
	}
    }
    
    if (cv_cp(new, cv) < 0) {
	clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
	goto quit;
    }
	
    retval = clicon_dbput(db, key, vec);

 quit:
    if (vec)
	cvec_free(vec);

    return retval;
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
    int retval = -1;

    if ((vec = clicon_dbget(db, key)) == NULL)
	goto quit;
    
    if ((cv = cvec_find(vec, variable)) == NULL)
	goto quit;

    cvec_del(vec, cv);
    retval = clicon_dbput(db, key, vec);

 quit:
    if (vec)
	cvec_free(vec);

    return retval;
}

/*! Merge a cvec into into db key
 *
 * Merge a cligen variable vector with the existing variable in database key.
 * Variables with matching names will be overwritten. If key does not exist
 * in database, a new db key will be created based on the new vector.
 *
 * @param   db       Name of database to search in (filename including dir path)
 * @param   key      Name of database key
 * @param   vec      Cligen variable vector
 */
int
clicon_dbmerge(char *db, char *key, cvec *vec)
{
    cvec *orig;
    cg_var *cv;
    cg_var *new;
    char *name;
    int retval = -1;

    if ((orig = clicon_dbget(db, key)) == NULL)
	return clicon_dbput(db, key, vec);

    cv = NULL;
    while ((cv = cvec_each (vec, cv))) {
    	if (cv_flag(cv, V_UNSET))  /* Allow for empty values as in db_spec */
	    continue;

	name = cv_name_get(cv);
	if (name && (new = cvec_find(orig, name)) != NULL) {
	    cv_reset(new);	
	} else {
	    if ((new = cvec_add(orig, cv_type_get(cv))) == NULL) {
		clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
		goto quit;
	    }
	}
	
	if (cv_cp(new, cv) < 0) {
	    clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
	    goto quit;
	}
    }

    retval = clicon_dbput(db, key, orig);

 quit:
    cvec_free(orig);
    
    return retval;
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
    int retval = -1;
    
    if ((vec = clicon_dbget(db, key)) == NULL)
	return -1;
    
    if ((new = cvec_add(vec, cv_type_get(cv))) == NULL) {
	clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
	goto quit;
    }
    if (cv_cp(new, cv) < 0) {
	clicon_err(OE_UNIX, errno, "%s:", __FUNCTION__);
	goto quit;
    }

    retval = clicon_dbput(db, key, vec);

 quit:
    if (vec)
	cvec_free(vec);
    
    return retval;
}


/*! Get list of db keys
 *
 * Get list of keys in database, excluding special vector keys. The list is a
 * NULL terminated array of char*
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
    size_t buflen;
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

    buflen = (nkeys+1) * sizeof(char *) + keylen;
    if ((keys = malloc(align4(buflen))) == NULL) {
	clicon_err(OE_UNIX, errno, "%s: malloc", __FUNCTION__);
	goto quit;
    }
    memset(keys, 0, buflen);

    n = 0;
    ptr = (char *)&keys[nkeys+1];
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



/*! Get list of db items
 *
 * Get list of items in database, excluding special vector keys. The list
 * is a NULL terminated array of cvec* where each cvec is named with the 
 * database key.
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

    /* Allocate list. One extra to NULL terminate list */
    if ((items = calloc(nkeys+1, sizeof(cvec *))) == NULL) { 
	clicon_err(OE_UNIX, errno, "%s: calloc", __FUNCTION__);
	goto quit;
    }
    memset(items, 0, (nkeys+1) * sizeof(cvec *));

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


/*! Free list of db items
 *
 * Free list of database items as allocated by clicon_dbitems()
 *
 * @param   vecs     List of cvec pointers
 */
void
clicon_dbitems_free(cvec **items)
{
    int i;

    for (i = 0; items[i]; i++)
	cvec_free(items[i]);
    free(items);
}
