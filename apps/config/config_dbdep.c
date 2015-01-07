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

 */

#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <netinet/in.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include <clicon/clicon.h>

#include "clicon_backend_api.h"
#include "config_dbdiff.h"
#include "config_dbdep.h"
#include "config_handle.h"

/*
 * Free database depencency
 */
static void
dbdep_free(dbdep_t *dp)
{
    dbdep_ent_t *dpe;

    while((dpe = dp->dp_ent) != NULL) {
	DELQ(dpe, dp->dp_ent, dbdep_ent_t *);
	free(dpe->dpe_key);
	if (dpe->dpe_var)
	    free(dpe->dpe_var);
	free(dpe);
    }
    free(dp);
}

/*
 * Free a database dependency list.
 */
void
dbdeps_free(clicon_handle h)
{
    dbdep_t *dp;
    dbdep_t *deps = backend_dbdep(h);

    while((dp = deps) != NULL) {
	DELQ(dp, deps, dbdep_t *);
	dbdep_free(dp);
    }
    backend_dbdep_set(h, NULL);
}

/*
 * Create empty dependency component
 */
static dbdep_t *
dbdep_create()
{
    dbdep_t *dp;

    if ((dp = malloc(sizeof(dbdep_t))) == NULL) {
	clicon_err(OE_DB, errno, "malloc: %s", strerror(errno));
	return NULL;
    }
    memset (dp, 0, sizeof (*dp));
    dp->dp_row = (uint16_t)-1;

    return dp;
}


/*
 * Add a key+var dependency entry to a dependency component
 */
static int
dbdep_ent(dbdep_handle_t dh, const char *key, const char *var)
{
    dbdep_ent_t *dpe;
    dbdep_t *dp = (dbdep_t *)dh;
    
    if((dpe = malloc(sizeof(*dpe))) == NULL) {
	clicon_err(OE_DB, errno, "malloc: %s", strerror(errno));
	goto err;
    }
    memset(dpe, 0, sizeof(*dpe));
    
    if ((dpe->dpe_key = strdup(key)) == NULL ||
	(var && (dpe->dpe_var = strdup(var)) == NULL) ) {
	clicon_err(OE_DB, errno, "malloc: %s", strerror(errno));
	goto err;
    }
    
    ADDQ(dpe, dp->dp_ent);
    return 0;
    
err:
    if(dpe) {
	if (dpe->dpe_key)
	    free(dpe->dpe_key);
	if (dpe->dpe_var)
	    free(dpe->dpe_var);
	free(dpe);
    }
    return -1;
} 

/*! Create plugin validate dependency and register callback 
 *
 * Create config validate dependency and register a callback with argument. 
 * The dependency specifies a database key or key pattern (eg "a[].b*"). A callback with the 
 * following signature is registered:
 *   cb(h, dbname, op, key, arg);
 * where 'h' is the clicon handle, dbname is the database (eg running), op is delete or set,
 * and arg is the argument specified in this function.
 * Whenever a key matching the pattern (eg a.0.b.8, a.0.b.8.c), cb will be 
 * for each matching key:
 *   cb(h, dbname, op, a.0.b.8, arg);
 *   cb(h, dbname, op, a.0.b.8.c, arg);
 *
 * @param  h     Config handle
 * @param  prio  Priority. The lower the number, the earlier the callback is called.
 * @param  cb    Function to call
 * @param  arg   Arg to send as parameter to callback
 * @param  key   A database key or key pattern. 
 *
 * @retval NULL  Error
 * @retval dp    Dependency handle.
 * @code
 * dbdep(h, 0, mycommit, NULL, "a[].b*");
 * @endcode
 * @see dbdep, dbdep_tree
 */
dbdep_handle_t
dbdep_validate(clicon_handle h, /* Config handle */
	       uint16_t      prio, 
	       trans_cb      cb,            /* Callback called */
	       void         *arg,           /* Arg to send to callback */
	       char         *key)           /* Key we're depending on */
{
    dbdep_t *dp, *deps;
    char    *var;

    if ((dp = dbdep_create()) == NULL)
	return NULL;
    dp->dp_row = prio;
    dp->dp_deptype = DBDEP_KEY;
    dp->dp_callback = cb;
    dp->dp_arg  = arg;
    dp->dp_type = TRANS_CB_VALIDATE;
    
    clicon_debug(2, "%s: Created validation dependency '%s'", __FUNCTION__, key);
    if ((var = strchr(key, ':')))
      *var++ = '\0';
    if (dbdep_ent(dp, key, var) < 0)
      goto catch;
    if (var)
      *(var-1) = ':';

    deps = backend_dbdep(h); /* ADDQ must work w an explicit variable */
    ADDQ(dp, deps);
    backend_dbdep_set(h, deps);
    return dp;

catch:
    if (dp)
	dbdep_free(dp);
    return NULL;
}

/*! Create recursive plugin validate dependency and register callback
 *
 * Create recursive validate dependency and register a callback with argument. 
 * The dependency specifies a database key or key pattern (eg "a[].b*"). A callback with the 
 * following signature is registered:
 *   cb(h, dbname, op, key, arg);
 * where 'h' is the clicon handle, dbname is the database (eg running), op is delete or set,
 * and arg is the argument specified in this function.
 * Whenever a key matching the pattern (eg a.0.b.8, a.0.b.8.c), a single call to cb will be 
 * made:
 *   cb(h, dbname, op, a.0.b, arg);
 * The difference with dbdep_validate() is that only a single callback is made
 *
 * @param  h     Config handle
 * @param  prio  Priority. The lower the number, the earlier the callback is called.
 * @param  cb    Function to call
 * @param  arg   Arg to send as parameter to callback
 * @param  key   A database key or key pattern.
 *
 * @retval NULL  Error
 * @retval dp    Dependency handle.
 * @code
 * dbdep_tree(h, 0, mycommit, NULL, "a[].b*");
 * @endcode
 * @see dbdep, dbdep_validate
 */
dbdep_handle_t
dbdep_tree_validate(clicon_handle h, /* Config handle */
		    uint16_t      prio, 
		    trans_cb      cb,            /* Callback called */
		    void         *arg,           /* Arg to send to callback */
		    char         *key)           /* Key we're depending on */
{
  dbdep_t *dp, *deps;
    char    *var;

    if ((dp = dbdep_create()) == NULL)
	return NULL;
    dp->dp_row = prio;
    dp->dp_deptype = DBDEP_TREE;
    dp->dp_callback = cb;
    dp->dp_arg  = arg;
    dp->dp_type = TRANS_CB_VALIDATE;
    
    clicon_debug(2, "%s: Created validation dependency '%s'", __FUNCTION__, key);
    if ((var = strchr(key, ':')))
      *var++ = '\0';
    if (dbdep_ent(dp, key, var) < 0)
      goto catch;
    if (var)
      *(var-1) = ':';

    deps = backend_dbdep(h); /* ADDQ must work w an explicit variable */
    ADDQ(dp, deps);
    backend_dbdep_set(h, deps);
    return dp;

catch:
    if (dp)
	dbdep_free(dp);
    return NULL;
}

/*! Create recursive plugin commit dependency and register callback
 *
 * Create recursive config dependency and register a callback with argument. 
 * The dependency specifies a database key or key pattern (eg "a[].b*"). A callback with the 
 * following signature is registered:
 *   cb(h, dbname, op, key, arg);
 * where 'h' is the clicon handle, dbname is the database (eg running), op is delete or set,
 * and arg is the argument specified in this function.
 * Whenever a key matching the pattern (eg a.0.b.8, a.0.b.8.c), a single call to cb will be 
 * made:
 *   cb(h, dbname, op, a.0.b, arg);
 * The difference with dbdep() is that only a single callback is made
 *
 * @param  h     Config handle
 * @param  prio  Priority. The lower the number, the earlier the callback is called.
 * @param  cb    Function to call
 * @param  arg   Arg to send as parameter to callback
 * @param  key   A database key or key pattern.
 *
 * @retval NULL  Error
 * @retval dp    Dependency handle.
 * @code
 * dbdep_tree(h, 0, mycommit, NULL, "a[].b*");
 * @endcode
 * @see dbdep, dbdep_validate
 */

dbdep_handle_t
dbdep_tree(clicon_handle h, /* Config handle */
	   uint16_t      prio, 
	   trans_cb      cb,            /* Callback called */
	   void         *arg,           /* Arg to send to callback */
	   char         *key)           /* Key we're depending on */
{
    dbdep_t *dp, *deps;
    char    *var;

    if ((dp = dbdep_create()) == NULL)
	return NULL;
    dp->dp_row = prio;
    dp->dp_deptype = DBDEP_TREE;
    dp->dp_callback = cb;
    dp->dp_arg  = arg;
    dp->dp_type = TRANS_CB_COMMIT;
    
    clicon_debug(2, "%s: Created tree dependency '%s'", __FUNCTION__, key);
    if ((var = strchr(key, ':')))
      *var++ = '\0';
    if (dbdep_ent(dp, key, var) < 0)
      goto catch;
    if (var)
      *(var-1) = ':';

    deps = backend_dbdep(h); /* ADDQ must work w an explicit variable */
    ADDQ(dp, deps);
    backend_dbdep_set(h, deps);
    return dp;

catch:
    if (dp)
	dbdep_free(dp);
    return NULL;
}

/*! Create plugin commit dependency and register callback 
 *
 * Create config dependency and register a callback with argument. 
 * The dependency specifies a database key or key pattern (eg "a[].b*"). A callback with the 
 * following signature is registered:
 *   cb(h, dbname, op, key, arg);
 * where 'h' is the clicon handle, dbname is the database (eg running), op is delete or set,
 * and arg is the argument specified in this function.
 * Whenever a key matching the pattern (eg a.0.b.8, a.0.b.10.c), cb will be 
 * for each matching key as follows:
 *   cb(h, dbname, op, a.0.b.8, arg);
 *   cb(h, dbname, op, a.0.b.10.c, arg);
 * The difference with dbdep_tree is that a callback for each changed key will be made.
 *
 * @param  h     Config handle
 * @param  prio  Priority. The lower the number, the earlier the callback is called.
 * @param  cb    Function to call
 * @param  arg   Arg to send as parameter to callback
 * @param  key   A database key or key pattern.
 *
 * @retval NULL  Error
 * @retval dp    Dependency handle.
 * @code
 *   dbdep(h, 0, mycommit, NULL, "a[].b*");
 * @endcode
 * @see dbdep_tree, dbdep_validate
 */
dbdep_handle_t
dbdep(clicon_handle h,             /* Config handle */
      uint16_t      prio, 
      trans_cb      cb,            /* Callback called */
      void         *arg,           /* Arg to send to callback */
      char         *key)           /* Key we're depending on */
{
    dbdep_t *dp, *deps;
    char    *var;

    if ((dp = dbdep_create()) == NULL)
	return NULL;
    dp->dp_row      = prio;
    dp->dp_deptype  = DBDEP_KEY;
    dp->dp_callback = cb;
    dp->dp_arg      = arg;
    dp->dp_type     = TRANS_CB_COMMIT;
    
    clicon_debug(2, "%s: Created dependency '%s'", __FUNCTION__, key);
    if ((var = strchr(key, ':')))
      *var++ = '\0';
    if (dbdep_ent(dp, key, var) < 0)
      goto catch;
    if (var)
      *(var-1) = ':';

    deps = backend_dbdep(h); /* ADDQ must work w an explicit variable */
    ADDQ(dp, deps);
    backend_dbdep_set(h, deps);
    return dp;

catch:
    if (dp)
	dbdep_free(dp);
    return NULL;
}



/*
 * spec_key_index
 * eat numerical tokens from key.
 * key is expected to be on the form '123.a.b', eat until '.a.b'
 */
static int
match_key_index(int *i0, char *key)
{
    int i = *i0;

    while (isdigit(key[i]))
	i++;
    if (i == *i0)
	return 0; /* fail */
    i--;
    *i0 = i;
    return 1;
}


/*
 * dbdep_match_key
 * key on the form A.4.B.4
 * skey on the form A[].B[]
 * return 0 on fail, 1 on sucess.
 * key:  a.0b
 * skey: a[]
 */
static int
dbdep_match_key(char *key, char *skey, char **matched)
{
    int   i, j;
    char  k, s;
    int   vec;

    vec = 0;
    for (i=0, j=0; i<strlen(key) && j<strlen(skey); i++, j++){
	k = key[i];
	s = skey[j];
	if (s == '*') 
	    goto ok;

	if (vec){
	    vec = 0;
	    if (s == ']') /* skey has [] entry */
		if (match_key_index(&i, key) == 1)
		    continue; /* ok */
	    break; /* fail */
	}
	else{
	    if (s == '[' && k == '.')
		vec++;
	    else
		if (k != s)
		    break; /* fail */
	}
    } /* for */
    if (i>=strlen(key) && j>=strlen(skey) && vec==0)
        goto ok; /* ok */
    else if (i>=strlen(key) && j==strlen(skey)-1 && vec==0)
        goto ok; /* wildcard, ok */
    else
	return 0; /* fail */

 ok:
    if (matched) {
        if ((*matched = strdup(key)) != NULL)
	  (*matched)[i] = '\0';
    }
    return 1;
}

/*
 * Match an actual database key to a key in a dependency entry.
 */
static dbdep_ent_t *
dbdep_match(dbdep_ent_t *dent, const char *dbkey, char **match)
{
#if 0
    char *key;
    int status;
    regex_t re;
#endif
    dbdep_ent_t *dpe;
    dbdep_ent_t *retval = NULL;

    dpe = dent;
    do {
        if (dbdep_match_key((char*)dbkey, dpe->dpe_key, match)){
	    retval = dpe;
	    break;
	}
	dpe = NEXTQ(dbdep_ent_t *, dpe);
    } while (dpe != dent);

/*    unchunk_group(__FUNCTION__);*/
    return retval;
}

/*
 * Dependency qsort fun.
 */
static int
dbdep_commitvec_sort(const void *arg1, const void *arg2)
{
    dbdep_dd_t *d1 = (dbdep_dd_t *)arg1;
    dbdep_dd_t *d2 = (dbdep_dd_t *)arg2;

    return d1->dd_dep->dp_row - d2->dd_dep->dp_row;
}


/*
 * dbdep_commitvec
 * Create commit vector based of dbdiff result.
 * return pointer to (malloced) vector in vec with lengtrh nvecp (needs to be
 * freed after use)
 * Input:
 *   h      config handle. Retrieve dbdep list from this.
 *   dd     Vector of database differences: (key in db1, key in db2, add/del/mod)*
 * Output:
 *   nvecp  Length of return vector
 *   vecp   Pairs of dbdep and dbdiff entries. Ie, if key "A" has dependency in 
 *          plugin X and Y. Vector will contain: [(X A) (Y A)]
 * Return 0  on OK, on error return -1
 */
int
dbdep_commitvec(clicon_handle h, 
		const struct dbdiff *dd, 
		int *nvecp, 
		dbdep_dd_t **ddvec0)
{
    int         i, j;
    int         nvec;
    dbdep_dd_t *ddvec;
    char       *key;
    dbdep_t    *dp;
    dbdep_t    *deps;
    char       *match;
    char       *ddkey;

    nvec = 0;
    ddvec = NULL;
    if ((deps = backend_dbdep(h)) == NULL) /* No dependencies registered, OK */
	goto done;
    
    for (i = 0; i < dd->df_nr; i++) {

	/* We only have to check one key. Vector keys may have different 
	   vector index making the actual keys different, but they both
	   relate to the same spec-key and therefore the same config component
	*/
        if (dd->df_ents[i].dfe_op & DBDIFF_OP_FIRST)
	    key = dd->df_ents[i].dfe_key1;
	else
	    key = dd->df_ents[i].dfe_key2;

	/* Match any config component matching the key and add to vector */
	dp = deps;
	do {
	   if (dp->dp_ent && dbdep_match (dp->dp_ent, key, &match)) {
	       if (dp->dp_deptype == DBDEP_TREE)  {/* If tree depencency, Check for uniqueness */
		    for (j = 0; j < nvec; j++) {
		        if (ddvec[j].dd_dbdiff->dfe_op & DBDIFF_OP_FIRST)
			    ddkey = ddvec[j].dd_mkey1;
			else
			    ddkey = ddvec[j].dd_mkey2;
			if (ddvec[j].dd_dep == dp && match && strcmp(match, ddkey) == 0) {
			    free(match);
			    goto skip;
			}
		    }
	       }
	       if(match)
		   free(match);
	       if ((ddvec = realloc(ddvec, (nvec+1) * sizeof(*ddvec))) == NULL){
		  clicon_err(OE_DB, errno, "%s: realloc", __FUNCTION__);
		    goto err;
		}
		memset(&ddvec[nvec], 0, sizeof(ddvec[nvec]));
		ddvec[nvec].dd_dep = dp;
		ddvec[nvec].dd_dbdiff  = &dd->df_ents[i];
	        if (dp->dp_deptype == DBDEP_TREE) { /* Get matched part of key */
		    if (dd->df_ents[i].dfe_key1) {
		        dbdep_match_key (dd->df_ents[i].dfe_key1, dp->dp_ent->dpe_key, &ddvec[nvec].dd_mkey1);
		        if (ddvec[nvec].dd_mkey1 == NULL) {
			    clicon_err(OE_DB, errno, "%s: strdup", __FUNCTION__);
			    goto err;
			}
		    }
		    if (dd->df_ents[i].dfe_key2) {
		        dbdep_match_key (dd->df_ents[i].dfe_key2, dp->dp_ent->dpe_key, &ddvec[nvec].dd_mkey2);
		        if (ddvec[nvec].dd_mkey2 == NULL) {
			    clicon_err(OE_DB, errno, "%s: strdup", __FUNCTION__);
			    goto err;
			}
		    }
		} else {
		    if (dd->df_ents[i].dfe_key1)
		      if ((ddvec[nvec].dd_mkey1 = strdup(dd->df_ents[i].dfe_key1)) == NULL) {
			clicon_err(OE_DB, errno, "%s: strdup", __FUNCTION__);
			goto err;
		      }
		    if (dd->df_ents[i].dfe_key2)
		      if ((ddvec[nvec].dd_mkey2 = strdup(dd->df_ents[i].dfe_key2)) == NULL) {
			clicon_err(OE_DB, errno, "%s: strdup", __FUNCTION__);
			goto err;
		      }
		}
		nvec++;
	
	    }
	skip:
	    dp = NEXTQ(dbdep_t *, dp);
	} while (dp != deps);
    }
    
    /* Now sort vector based on dbdep row number */
    qsort(ddvec, nvec, sizeof(*ddvec), dbdep_commitvec_sort);

  done:
    *nvecp = nvec;
    *ddvec0 = ddvec;
    return 0;
err:
    if (ddvec)
	free(ddvec);
    return -1;
}


/*
 * dbdep_commitvec_free - Free the 
 */
void
dbdep_commitvec_free(dbdep_dd_t *ddvec, int nvec)
{
    int i;
  
    for (i=0; i < nvec; i++) {
      free(ddvec[i].dd_mkey1);
      free(ddvec[i].dd_mkey2);
    }
    free(ddvec);
}
