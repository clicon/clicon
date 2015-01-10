
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
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <syslog.h>
#include <ctype.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include "clicon_err.h"
#include "clicon_queue.h"
#include "clicon_string.h"
#include "clicon_chunk.h"
#include "clicon_hash.h"
#include "clicon_db.h"
#include "clicon_handle.h"
#include "clicon_dbspec_key.h"
#include "clicon_log.h"
#include "clicon_lvalue.h"
#include "clicon_yang.h"
#include "clicon_options.h"
#include "clicon_proto_client.h"
#include "clicon_dbmatch.h"
#include "clicon_dbutil.h"

/*
 * dbmatch_fn()
 * Look in the database and match entries according to a matching expression,
 * Matching expression is a variable and a shell-wildcard value.
 * Example: Database includes the following entries:
 * Key.0 $!a=442 $b=3 $uuid=u0
 * Key.1 $!a=443 $b=7 $uuid=u1
 * Key.1 $!a=53  $b=3 $uuid=u2
 *  dbmatch(dbname, key="^Key.*$", attr="a", pattern="44*", fn=cb, arg=3)
 * will result in the following calls:
 *  cb(h, dbname, "Key.0", vr, arg);
 *  cb(h, dbname, "Key.1", vr, arg);
 * attr=NULL selects all
 * Note: The callback (fn) returns 0 on success, -1 on error (and break), 1 on
 * break.
 * XXX: Extend pattern beyond attr=<pattern>
 * XXX: It is not assured if updating the database is occurred while the iteration.
 */
int
dbmatch_fn(void *handle,
	char *dbname, 
	char *keypattern, 
	char *attr, 
	char *pattern, 
	dbmatch_fn_t fn,
	void *fnarg,
	int  *matches) /* How many matches, 0 if none */

{
    struct db_pair  *pairs;
    int              npairs;
    char            *key;
    cvec            *vr = NULL;
    cg_var          *cv;
    int              match=0;
    int              retval = -1;
    char            *str;
    int              len;
    int              ret;

    /* Following can be done generic */
    if ((npairs = db_regexp(dbname, keypattern, 
			    __FUNCTION__, &pairs, 0)) < 0)
	goto done;
    for (npairs--; npairs >= 0; npairs--) {
	key = pairs[npairs].dp_key;
	if (key_isvector_n(key) || key_iskeycontent(key))
	    continue;
	if ((vr = dbkey2cvec(dbname, key)) == NULL) /* get cvec of key */
	    goto done;
	/* match attribute value with corresponding variable in database */
	if (attr){ /* attr and value given on command line */
	    if ((cv = cvec_find_var(vr, attr)) == NULL){
		cvec_free(vr);
		vr=NULL;
		continue; /* no such variable for this key */
	    }
	    if ((len = cv2str(cv, NULL, 0)) < 0)
		goto done;
	    if (len == 0){
		cvec_free(vr);
		vr=NULL;
		continue; /* If attr has no value (eg "") interpret it as no match */
	    }
	    if ((str = cv2str_dup(cv)) == NULL)
		goto done;
	    if(fnmatch(pattern, str, 0) != 0) {
		cvec_free(vr);
		vr=NULL;
		free(str);
		continue; /* no match */
	    }
	    free(str);
	    str = NULL;
	}
	match++;
	if (fn){
	    if ((ret = (*fn)(handle, dbname, key, vr, fnarg)) < 0)
		goto done;
	    if (ret == 1)
		break; /* return value 0 -> continue */
	}
	cvec_free(vr);
	vr = NULL;
    }
    if (matches)
	*matches = match;
    retval = 0;
  done:
    if (vr)
	cvec_free(vr);
    unchunk_group(__FUNCTION__);
    return retval;
}

/*! Return matching database entries using an attribute and string pattern
 *
 * @code
    int          len;
    char       **keyv;
    cvec       **cvecv;
    if (dbmatch_vec(h, dbname, "^Sender.*$", attr, val, 
		    &keyv, &cvecv, &len) < 0)
	goto done;
    for (i=0; i<len; i++)
       keyv[i],... cvecv[i],....
    dbmatch_vec_free(keyv, cvecv, len);
 * @endcode
 */
int
dbmatch_vec(void *handle,
	    char *dbname, 
	    char *keypattern, 
	    char *attr, 
	    char *pattern, 
	    char ***keyp,
	    cvec ***cvecp,
	    int  *lenp) /* How many matches, 0 if none */
    
{
    struct db_pair  *pairs;
    int              npairs;
    char            *key;
    char            **keyv = NULL;
    cvec            **cvecv = NULL;
    cvec            *vr = NULL;
    cg_var          *cv;
    int              match=0;
    int              retval = -1;
    char            *str;
    int              len;

    /* Following can be done generic */
    if ((npairs = db_regexp(dbname, keypattern, 
			    __FUNCTION__, &pairs, 0)) < 0)
	goto done;
    for (npairs--; npairs >= 0; npairs--) {
	key = pairs[npairs].dp_key;
	if (key_isvector_n(key) || key_iskeycontent(key))
	    continue;
	if ((vr = dbkey2cvec(dbname, key)) == NULL) /* get cvec of key */
	    goto done;
	/* match attribute value with corresponding variable in database */
	if (attr){ /* attr and value given on command line */
	    if ((cv = cvec_find_var(vr, attr)) == NULL){
		cvec_free(vr);
		vr=NULL;
		continue; /* no such variable for this key */
	    }
	    if ((len = cv2str(cv, NULL, 0)) < 0)
		goto done;
	    if (len == 0){
		cvec_free(vr);
		vr=NULL;
		continue; /* If attr has no value (eg "") interpret it as no match */
	    }
	    if ((str = cv2str_dup(cv)) == NULL)
		goto done;
	    if(fnmatch(pattern, str, 0) != 0) {
		cvec_free(vr);
		vr=NULL;
		free(str);
		continue; /* no match */
	    }
	    free(str);
	    str = NULL;
	}
	match++;
	if ((keyv = realloc(keyv, sizeof(char*) * match)) == NULL){	
	    clicon_err(OE_DB, errno, "%s: realloc", __FUNCTION__);
	    goto done;
	}
	if ((keyv[match-1] = strdup4(key)) == NULL){
	    clicon_err(OE_DB, errno, "%s: strdup", __FUNCTION__);
	    goto done;
	}
	if ((cvecv = realloc(cvecv, sizeof(cvec *) * match)) == NULL){	
	    clicon_err(OE_DB, errno, "%s: realloc", __FUNCTION__);
	    goto done;
	}
	cvecv[match-1] = vr;
	vr = NULL;
    }
    if (lenp)
	*lenp = match;
    if (cvecp){
	*cvecp = cvecv;
	cvecp = NULL;
    }
    if (keyp){
	*keyp = keyv;
	keyp = NULL;
    }
    retval = 0;
  done:
    if (vr)
	cvec_free(vr);
    if (cvecp)
	free(cvecp);
    if (keyp)
	free(keyp);
    unchunk_group(__FUNCTION__);
    return retval;
}

/*! Free all structures allocated by dbmatch_vec()
 */
int
dbmatch_vec_free(char **keyv, cvec **cvecv, int len)
{
    int i;

    for (i=0; i<len; i++){
	cvec_free(cvecv[i]);
	free(keyv[i]);
    }
    free(cvecv);
    free(keyv);
    return 0;
}

/*! Look in the database and match first entry according to a matching expression,
 * Matching expression is a variable and a shell-wildcard value.
 * Example: Database includes the following entries:
 * Key.0 $!a=442 $b=3 $uuid=u0
 * Key.1 $!a=443 $b=7 $uuid=u1
 * Key.1 $!a=53  $b=3 $uuid=u2
 *  dbmatch_one(dbname, key="^Key.*$", attr="a", pattern="44*", &key, &cvec)
 * will result in the return of the cvec of Key.0
 * This is a special case of dbmatch_fn()
 * NOTE: deallocate return values cvecp and keyp:
 * cvec_free(cvecp); free(keyp)
 */
int
dbmatch_one(void *handle,
	    char *dbname, 
	    char *keypattern, 
	    char *attr, 
	    char *pattern,
	    char **keyp,
	    cvec **cvecp
    )
{
    struct db_pair  *pairs;
    int              npairs;
    char            *key;
    cvec            *vr = NULL;
    cg_var          *cv;
    char            *str;
    int              len;
    int              retval = -1;

    /* Following can be done generic */
    if ((npairs = db_regexp(dbname, keypattern, 
			    __FUNCTION__, &pairs, 0)) < 0)
	goto done;
    for (npairs--; npairs >= 0; npairs--) {
	key = pairs[npairs].dp_key;
	if (key_isvector_n(key) || key_iskeycontent(key))
	    continue;
	if ((vr = dbkey2cvec(dbname, key)) == NULL) /* get cvec of key */
	    goto done;
	/* match attribute value with corresponding variable in database */
	if (attr){ /* attr and value given on command line */
	    if ((cv = cvec_find_var(vr, attr)) == NULL){
		cvec_free(vr);
		continue; /* no such variable for this key */
	    }
	    if ((len = cv2str(cv, NULL, 0)) < 0)
		goto done;
	    if (len == 0){
		cvec_free(vr);
		vr=NULL;
		continue; /* If attr has no value (eg "") interpret it as no match */
	    }
	    if ((str = cv2str_dup(cv)) == NULL)
		goto done;
	    if(fnmatch(pattern, str, 0) != 0) {
		cvec_free(vr);
		vr=NULL;
		free(str);
		continue; /* no match */
	    }
	    free(str);
	    str = NULL;
	}
	if (cvecp)
	    *cvecp = vr;
	if (keyp)
	    if ((*keyp = strdup4(key)) == NULL){
		clicon_err(OE_DB, errno, "%s: strdup", __FUNCTION__);
		goto done;
	    }
	break;
    }
    retval = 0;
  done:
    unchunk_group(__FUNCTION__);
    return retval;
}


/*! Given a key array, return a NULL terminated list of actual keys
 *
 * Given a vector key in 'key[]' format, return a NULL terminated
 * list of actual keys found in the database. Size of list is returned 
 * in 'len'. On error, NULL is returned.
 *
 * @param[in]  dbname 
 * @param[in]  basekey  
 * @param[out] len      Length of returned vector.
 * @retval     list     Allocated list of keys

 * NOTE: return value must be freed after use.
 * @code
 * char **vec = dbvectorkeys(running, "key[]", &len);
 * ...
 * free(vec);
 * @endcode
 */
char **
dbvectorkeys(char *dbname, char *basekey, size_t *len)
{
    int i;
    size_t totlen;
    char *ptr;
    char *vkey;
    char **list;
    int npairs;
    struct db_pair *pairs;

    list = NULL;
    *len = -1;
    
    vkey = db_gen_rxkey(basekey, __FUNCTION__);
    if (vkey == NULL)
	goto quit;
    
    /* Get all keys/values for vector */
    npairs = db_regexp(dbname, vkey, __FUNCTION__, &pairs, 1);
    if (npairs < 0)
	goto quit;
    
    totlen = 0;
    for (i = 0; i < npairs; i++)
	totlen += (strlen(pairs[i].dp_key) + 1);
    
    if ((list = malloc(((npairs+1) * sizeof(char *)) + totlen)) == NULL)
	goto quit;
    memset(list, 0, ((npairs+1) * sizeof(char *)) + totlen);

    ptr = (char *)(list + npairs); /* First byte after char** vector */
    for (i = 0; i < npairs; i++) {
	list[i] = ptr;
	strcpy (ptr, pairs[i].dp_key);
	ptr += (strlen(pairs[i].dp_key) + 1);
    }
    unchunk_group(__FUNCTION__);

    *len = npairs;
    return list;
    
quit:
    if (list)
	free(list);
    unchunk_group(__FUNCTION__);
    return NULL;
}

/*! Delete objects given key and attribute pattern matching
 *
 * Specific function using dbmatch. NOTE: cannot be used by backend
 */
int
dbmatch_del(void *handle,
	    char *dbname, 
	    char *key, 
	    char *attr, 
	    char *pattern,
	    int  *lenp)
{
    char **keyv;
    cvec **cvecv;
    int    len;
    int    i;
    int    retval = -1;

    if (dbmatch_vec(handle, dbname, key, attr, pattern, &keyv, &cvecv, &len) < 0)
	goto done;
    for (i=0; i<len; i++)
	if (clicon_proto_change_cvec(handle, dbname, LV_DELETE, keyv[i], cvecv[i]) < 0)
	    goto done;
    dbmatch_vec_free(keyv, cvecv, len);
    if (lenp)
	*lenp = len;
    retval = 0;
  done:
    return retval;
}

