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
#include <limits.h>
#include <fnmatch.h>


#include <cligen/cligen.h>

/* clicon */
#include "clicon_err.h"
#include "clicon_queue.h"
#include "clicon_chunk.h"
#include "clicon_hash.h"
#include "clicon_handle.h"
#include "clicon_dbspec_key.h"
#include "clicon_yang.h"
#include "clicon_string.h"
#include "clicon_options.h"
#include "clicon_db.h"
#include "clicon_qdb.h"
#include "clicon_handle.h"
#include "clicon_lvalue.h"
#include "clicon_dbutil.h"
#include "clicon_xml.h"
#include "clicon_xsl.h"
#include "clicon_xml_map.h"

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
 * @retval cvv  A cligen variable vector containing all variables found. This vector
 *              contains no variables (length == 0) if key is not found.
 *              The cvec needs to be freed by cvec_free() by caller.
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
 * @param[in]  db     Name of database to search in (filename incl dir path)
 * @param[in]  rx     Regular expression for key matching
 * @param[out] cvv    Vector of keys
 * @param[out] len    Pointer where length of returned vector is stored.
 */
int
clicon_dbkeys(char *db, char *rx, char ***keysv, size_t *len)
{
    int retval = -1;
    int i;
    int n;
    int keylen;
    int npairs;
    int nkeys;
    size_t buflen;
    char *ptr;
    char **keys = NULL;
    struct db_pair *pairs;

    if ((npairs = db_regexp(db, rx, __FUNCTION__, &pairs, 1)) < 0)
	goto quit;

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
    *keysv = keys;
    retval = 0;
quit:
    unchunk_group(__FUNCTION__);
    if (retval < 0)
	free(keys);
    
    return retval;
}

/*! Get database entries given key pattern
 *
 * Get list of items in database, excluding special vector keys. The list
 * is an array of cvec* where each cvec is named with the database key.
 *
 * @param[in]  db    Name of database to search in (filename including dir path)
 * @param[in]  rx    Regular expression for key matching. NULL or "^.*$" match all
 * @param[out] cvv   Vector of database items
 * @param[out] len   Pointer where length of returned vector is stored.
 * @retval  items  Vector of cvecs. Free with clicon_dbitems_free()
 * @code
 *  cvec **items;
 *  if (clicon_dbitems(resultdb, keypattern, &items, &nkeys) < 0)
 *    goto err;
 *  for (i=0; i<nkeys; i++){
 *    if ((item = items[i]) != NULL)
 *    ... do stuff ...
 *  }
 *  clicon_dbitems_free(items, nkeys);
 * @endcode
 */
int
clicon_dbitems(char   *db, 
	       char   *rx, 
	       cvec ***cvv, 
	       size_t *len)
{
    return clicon_dbitems_match(db, rx, NULL, NULL, cvv, len);
}

/*! Get database entries given key and attribute patterns
 *
 * More specific variant of clicon_dbitems where a single attribute value is
 * matched as well.
 *
 * @param[in]  db    Name of database to search in (filename including dir path)
 * @param[in]  rx    Regular expression for key matching. NULL or "^.*$" match all
 * @param[in]  attr  Name of attribute whose values should match
 * @param[in]  val   Attribute value match pattern
 * @param[out] cvv   Vector of database items
 * @param[out] len   Pointer where length of returned vector is stored.
 * @retval     items  Vector of cvecs. Free with clicon_dbitems_free()
 * @code
    int          i=0, len;
    cvec       **cvecv;

    if (clicon_dbitems_match(dbname, "^Sender.*$", "name", "my*", 
                             &cvecv, &len) < 0)
	goto done;
    for (i=0; i<len; i++){
       do something with cvecv[i];
    }
    clicon_dbitems_free(cvecv, len);
 * @endcode
 * @see clicon_dbitems
 */
int
clicon_dbitems_match(char   *db, 
		     char   *rx, 	    
		     char   *attr, 
		     char   *pattern, 
		     cvec ***cvvp, 
		     size_t *lenp)
{
    int             i;
    int             n;
    int             npairs;
    int             retval = -1;
    cvec          **items = NULL;
    cvec           *cvv;
    cg_var         *cv;
    struct db_pair *pairs;
    size_t          len;
    char           *str;
    
    *lenp = 0;

    if ((npairs = db_regexp(db, rx, __FUNCTION__, &pairs, 0)) < 0)
	goto quit;
    
    n = 0;
    for (i = 0; i < npairs; i++) {
	if(key_isvector_n(pairs[i].dp_key) || key_iskeycontent(pairs[i].dp_key))
	    continue;
	if ((cvv = lvec2cvec(pairs[i].dp_val, pairs[i].dp_vlen)) == NULL)
	    goto quit;
	/* match attribute value with corresponding variable in database */
	if (attr){ /* attr and value given on command line */
	    if ((cv = cvec_find_var(cvv, attr)) == NULL){
		cvec_free(cvv);
		continue; /* no such variable for this key */
	    }
	    if ((len = cv2str(cv, NULL, 0)) < 0)
		goto quit;
	    if (len == 0){
		cvec_free(cvv);
		continue; /* If attr has no value (eg "") interpret it as no match */
	    }
	    if ((str = cv2str_dup(cv)) == NULL)
		goto quit;
	    if(fnmatch(pattern, str, 0) != 0) {
		cvec_free(cvv);
		free(str);
		continue; /* no match */
	    }
	    free(str);
	    str = NULL;
	}
	/* Allocate list. */
	if ((items = realloc(items, (n+1)*sizeof(cvec *))) == NULL) { 
	    clicon_err(OE_UNIX, errno, "%s: calloc", __FUNCTION__);
	    goto quit;
	}
	items[n] = cvv;
	if (cvec_name_set(items[n], pairs[i].dp_key) == NULL) {
	    clicon_err(OE_DB, 0, "%s: cvec_name_set", __FUNCTION__);
	    goto quit;
	}
	n++;
    }
    *lenp = n;
    retval = 0;
quit:
    unchunk_group(__FUNCTION__);
    if (retval != 0) {
	if (items) {
	    for (i = 0; i < n; i++)
		cvec_free(items[i]);
	    free(items);
	}
    }
    else
	*cvvp = items;
    return retval;
}

/*! Given a database key, get its parent, 
 * For example, if key="a.0.b" get db content for "a.0" 
 * @param[in]  db    Name of database
 * @param[in]  key   Database key
 * @param[out] cvv   Contains parent database symbol and content, NULL if top
 * Note free cvv with cvec_free() after use
 */
int
clicon_dbget_parent(char      *db, 
		    char      *key, 
		    cvec     **cvv)
{
    int   retval = -1;
    char *pkey = NULL;
    char *pos;
    int64_t i64;
    
    /* pkey is parent key: remove rightmost element, eg a.b -> a 
       or a.0 -> NULL
     */
    if ((pkey = strdup(key)) == NULL){
	clicon_err(OE_UNIX, errno, "malloc");
	goto done;
    }
    if ((pos = rindex(pkey, '.')) == NULL)
	goto ok; /* top */
    *pos = '\0';
    if (parse_int64(pos+1, &i64, NULL) == 1){
	if ((pos = rindex(pkey, '.')) == NULL)
	    goto ok; /* top */
	*pos = '\0';
    }
    *cvv = clicon_dbget(db, pkey);
 ok:
    retval = 0;
 done:
    if (pkey)
	free(pkey);
    return retval;
}

/*! Given a database key, return list of children in cvec form
 * @param[in]  db    Name of database
 * @param[in]  key   Database key
 * @param[out] cvv   Vector of children. Free with clicon_dbitems_free()
 * @param[out] len   Length of vector
 */
int
clicon_dbget_children(char      *db, 
		      char      *key, 
		      cvec    ***cvv, 
		      size_t    *len)
{
    int   retval = -1;
    char *rx;

    rx = chunk_sprintf(__FUNCTION__, 
		       "^%s%s[a-zA-Z0-9]*(\\.[0-9]*)?$", 
		       key?key:"", 
		       key?".":"");
    if (rx == NULL){
	clicon_err(OE_XML, errno, "chunk_sprintf");
	goto done;
    }
    if (clicon_dbitems(db, rx, cvv, len) < 0)
	goto done;
    retval = 0;
 done:
    unchunk_group(__FUNCTION__);
    return retval;
}

/*! Given a database key, return list of all descendants in cvec form
 * @param[in]  db    Name of database
 * @param[in]  key   Database key
 * @param[out] cvv   Vector of descendants. Free with clicon_dbitems_free()
 * @param[out] len   Length of vector
 */
int
clicon_dbget_descendants(char      *db, 
			 char      *key, 
			 cvec    ***cvv, 
			 size_t    *len)
{
    int   retval = -1;
    char *rx;

    rx = chunk_sprintf(__FUNCTION__, 
		       "%s%s.*$", 
		       key?key:"", 
		       key?".":"");
    if (rx == NULL){
	clicon_err(OE_XML, errno, "chunk_sprintf");
	goto done;
    }
    if (clicon_dbitems(db, rx, cvv, len) < 0)
	goto done;
    retval = 0;
 done:
    unchunk_group(__FUNCTION__);
    return retval;
}

static int
key2xpath(char *key, char **xpath)
{
    int     retval = -1;
    char   *key2 = NULL;
    int     nvec;
    char  **vec = NULL;
    char   *str;
    char   *rx = "";
    int     i;
    int64_t i64;

    if ((key2 = strdup(key)) == NULL){
	clicon_err(OE_UNIX, errno, "strdup");
	goto done;
    }
    if ((vec = clicon_sepsplit (key2, ".", &nvec, __FUNCTION__))==NULL){
	clicon_err(OE_UNIX, errno, "clicon_sepplit");
	goto done;
    }
    for (i = 0; i < nvec; i++) {
	str = vec[i];
	if (parse_int64(str, &i64, NULL) == 1)
	    continue;
	if (rx)
	    rx = chunk_sprintf(__FUNCTION__, "%s/%s", rx, str);
	else
	    rx = chunk_sprintf(__FUNCTION__, "%s", str);
    }
    if ((*xpath = strdup(rx)) == NULL){
	clicon_err(OE_UNIX, errno, "strdup");
	goto done;
    }
    retval = 0;
 done:
    if (key2 != NULL)
	free(key2);
    unchunk_group(__FUNCTION__);
    return retval;
}

/*! Access objects in database using xpath expressions
 * @param[in]  h       Clicon handle
 * @param[in]  cn      Represents 'current' position in tree, NULL if root.
 * @param[in]  dbname  Name of database file
 * @param[in]  xpath   xpath expression on the form /a/b[a=5]
 * @param[out] cn_list Result list of nodes Free with clicon_dbitems_free()
 * @param[out] cn_len  Length of vector
 * @retval     cn_list Result list of nodes 
 *
 * @code
 * clicon_dbget_xpath(h, db, cn, "//c[z=73]", &cn_list, &cn_len);
 * clicon_dbitems_free(cn_list, cn_len)
 * @endcode
 * @see xpath_vec
 * @see yang_xpath
 */
int
clicon_dbget_xpath(clicon_handle h, 
		   char         *dbname, 
		   cvec         *cn,  /* context node */
		   char         *xpath,
		   cvec       ***cn_list0,
		   int          *cn_len0
		   )
{
    int                   retval = -1;
    dbspec_key           *dbspec;
    cxobj                *x;
    cxobj                *x2;
    cxobj               **xv;
    cxobj                *xn;
    int                   i;
    char                 *key;
    char                 *dbkey;
    cvec                **cn_list = NULL;
    int                   cn_len=0;
    char                 *rx;
    char                 *xp2;

    key = cvec_name_get(cn);

    dbspec = clicon_dbspec_key(h);
    /* Two cases:
       (1) Start from  / for absolute paths
       (2) Start from cn a.0.b -> /a/b for relative paths
       XXX: dont work for ..
     */
    if (xpath && (*xpath=='/')) /* absolute paths */
	rx = chunk_sprintf(__FUNCTION__, "^%s.*$", "");
    else
	rx = chunk_sprintf(__FUNCTION__, "^%s.*$", key);
    if ((x = db2xml_key(dbname, dbspec, rx, "clicon")) == NULL) 
	goto done;
    if (key2xpath(key, &xp2) < 0)
	goto done;
    if ((x2 = xpath_first(x, xp2)) == NULL) 
	goto done;
    if ((xv = xpath_vec(x2, xpath, &cn_len)) != NULL) {
	/* Allocate list. One extra to NULL terminate list */
	if ((cn_list = calloc(cn_len+1, sizeof(cvec *))) == NULL){
	    clicon_err(OE_UNIX, errno, "calloc");
	    goto done;
	}
	for (i=0; i<cn_len; i++){
	    xn = xv[i];
	    dbkey = xml_dbkey(xn);
	    cn_list[i] = clicon_dbget(dbname, dbkey);
	}
    }
    retval = 0;
 done:
    unchunk_group(__FUNCTION__);
    xml_free(x);
    if (retval == 0){
	*cn_len0 = cn_len;
	*cn_list0 = cn_list;
    }
    else
	clicon_dbitems_free(cn_list, cn_len);
    return retval;
}


/*! Free list of db items
 *
 * Free list of database items as allocated by clicon_dbitems()
 * This is really 'free vector of cvec:s'
 *
 * @param   vecs     List of cvec pointers
 * @param   len      Length of vector
 */
void
clicon_dbitems_free(cvec **items, size_t len)
{
    int i;

    for (i = 0; i<len; i++)
	cvec_free(items[i]);
    free(items);
}
