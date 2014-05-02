
/*
 *  CVS Version: $Id: clicon_dbutil.c,v 1.30 2013/09/20 11:45:43 olof Exp $
 *
  Copyright (C) 2009-2013 Olof Hagsand and Benny Holmgren

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
/* 
 * Database utility functions
 * Here are functions not directly related to lvmaps or lvalues.
 * In particular, there are transformation functions. Database values
 * are translated between qdb format (continuous string) and 'lvalues'.
 * which in turn are linked together as vars. These in turn can
 * be translated to cligen variables (cgv:s) and vectors of cgv:s (cvec/vr).
 * Probably this can be simplified.
 * Here is a graph of these formats and some translation functions:
 *
 *                       (database)
 *         <-lvec2cvec   +-------+  
 *       +-------------> | lvec  | 
 *       | cvec2lvec->   +-------+ 
 *       |                         
 *       |                         
 *       v (cligen)                        
 *   +---------+         
 *   | cvec    |  
 *   +---------+         
 *   +---------+         <-lv2cv          +---------+
 *   |  cv     |  <---------------------> |lvalue/lv|
 *   +---------+          cv2lv->         +---------+
 *        |                                     ^
 *        |                                     |
 *        |<-cv_parse   +-----------+  <-lv2str |
 *        +-----------> |  string   | <---------+
 *         cv2str->     +-----------+
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
#include "clicon_spec.h"
#include "clicon_log.h"
#include "clicon_lvalue.h"
#include "clicon_options.h"
#include "clicon_proto_client.h"
#include "clicon_dbutil.h"

/*!
 * \brief Append a new cligen variable (cv) to cligen variable vector (cvec),
 *
 * Copy contents to new cv and return it. A utility function to cvec_add
 * See also cvec_add() 
 */
cg_var *
cvec_add_cv(cvec *vr, cg_var *cv)
{
    cg_var *new;

    if ((new = cvec_add(vr, cv_type_get(cv))) == NULL)
	return NULL;

    cv_cp(new, cv);
    return new;
}

/*!
 * \brief Merge two cvec's, no overlap.
 *
 * Arguments:
 *	orig		- Original variable vector
 *	add		- New variable vector
 *	overwrite	- If the same variable exist in both 'orig' and 'add',
 *			  should 'old' variables be overwritten by 'add'.
 *
 * Returns: Number of added/overwritten keys to 'old'
 */
int
cvec_merge(cvec *orig, cvec *add)
{
    int            retval = 0;
    cg_var        *cv;

    cv = NULL;
    while ((cv = cvec_each (add, cv))) {
	if (cv_flag(cv, V_UNSET))  /* Allow for empty values as in db_spec */
	    continue;
	if (cvec_find_var(orig, cv_name_get(cv)) != NULL) 
	    continue; /* If already there, leave it */
	if (cvec_add_cv(orig, cv) == NULL)
	    return -1;
	retval++;
    }
    return retval;
}

/*!
 * \brief Merge two cvec's, accept overlap
 *
 * Same as cvec_merge but same variable name may occur several times.
 * Example: assume variables eg x, y below 
 * The values in a leaf-list MUST be unique.
 *
 * vec:[x=42;y=99] old:[x=42;y=100] => vec:[x=42;y=99;x=42;y=100]
 * vec:[x=42;y=99] old:[x=42;y=100] => vec:[x=42;y=99;y=100] ??
 * vec:[x=42;y=99] old:[x=42;y=99]  => vec:[x=42;y=99]
 */
int
cvec_merge2(cvec *orig, cvec *add)
{
    int             retval = 0;
    cg_var         *cv;
    cg_var         *cvo; /* original */
    int             found;

    cv = NULL;
    while ((cv = cvec_each (add, cv))) {
	if (cv_flag(cv, V_UNSET))  /* Allow for empty values as in db_spec */
	    continue;
	cvo = NULL;
	found = 0;
	while ((cvo = cvec_each (orig, cvo))) {
	    if (strcmp(cv_name_get(cv), cv_name_get(cvo)))
		continue;
	    /* same name: let it be, dont overwrite */
	    if (cv_cmp(cv, cvo) == 0){ /* same name,val exists in orig, dont add */
		found++;
		break;
	    }
	    /* same name but different value */
	}
	if (!found){ /* If name,val not found in orig, add it to orig. */
	    if (cvec_add_cv(orig, cv) == NULL)
		goto err;
	    retval++;
	}
    }
    return retval;
  err:
    return -1;
}


/*
 * dbvar2cv
 * Find a variable in the db given its name, a db-name and a key and return 
 * a single value or NULL if not found. If it has many values, the 'first' 
 * is returned.
 * See also dbvar2cvec()
 * This is malloced so it must be freed (cv_free) by caller.
 */
cg_var *
dbvar2cv(char *dbname, char *key, char *variable)
{
    cg_var             *cv = NULL;
    cg_var             *cv0;
    cvec               *cvec = NULL;

    /* First read cv vector from database */
    if ((cvec = dbkey2cvec(dbname, key)) == NULL)
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

/*!
 * \brief Seacrh for key in database and return a vector of cligen variables.
 *
 * Find a key in the db and return its vector of values as a vector of cligen
 * variables.
 * Note: if key not found an empty cvec will be returned (not NULL).
 *
 * Args:
 *  IN   dname  Name of database to search in (filename including dir path)
 *  IN   key    String containing key to look for.
 * Returns:
 *  A cligen vector containing all variables found. This vector contains no 
 *  variables (length == 0) if key is not found.
 *  Returned cvec needs to be freed with cvec_free().
 */
cvec *
dbkey2cvec(char *dbname, char *key)
{
    size_t           len;
    char            *lvec = NULL;
    cvec            *cvec = NULL;

    /* Dont expand vector indexes, eg A.n in a A[] entry */
    if(key_isvector_n(key) || key_iskeycontent(key))
	goto catch;
    /* Read key entry from database */
    if (db_get_alloc(dbname, key, (void*)&lvec, &len) < 0)
	goto catch;
    cvec = lvec2cvec(lvec, len);
catch:
    if(lvec)
	free(lvec);
    return cvec;
}

/*
 * cvec2dbkey
 * Write a key+cvec to database.
 */
int 
cvec2dbkey(char *dbname, char *key, cvec *cvec)
{
    char            *lvec = NULL;
    size_t           lvlen;

    /* Transform variable list to contiguous char vector */
    if ((lvec = cvec2lvec(cvec, &lvlen)) == NULL)
	return -1;
    /* Write to database, key and a vector of variables */
    if (db_set(dbname, key, lvec, lvlen) < 0)
	return -1;
    return 0;
}

/*
 * lvec2cvec
 * Translate from lvec to cgv cvec
 * The cvec needs to be freed by cvec_free by the caller.
 * Note, the special vector entry of type "A.n key:number" 
 * is a meta entry for vectors. The caller needs to ensure that these are not called.
 */
cvec *
lvec2cvec(char *lvec, size_t len)
{
    cvec          *retval = NULL;
    struct lvalue *val;
    struct lvalue *lvi;
    int            hlen;
    char          *name = NULL;
    int            i, nr;
    cg_var        *cv;
    cvec          *vr = NULL;
    int            unique = 0;

    /* Compute number of elements */
    nr = 0;
    val = lvi = (struct lvalue *)lvec;
    hlen = (void*)lvi->lv_val - (void*)lvi;
    while ((void *)val < (void *)lvi+len) {
	nr++;
	val = (struct lvalue *)((void *)val + hlen+val->lv_len);
    }
#if 0 /* For keys without variables */
    if (nr==0){
	clicon_log(LOG_WARNING, "%s: lvec len should be nonzero: %d\n", __FUNCTION__, nr);
	goto catch;
    }
#endif
    if (nr%2){ 	/* silently drop: see note, this could be a vector entry */
	clicon_log(LOG_WARNING, "%s: lvec len should be even: %d\n", __FUNCTION__, nr);
	goto catch;
    }
    if ((vr = cvec_new(nr/2)) == NULL){
	fprintf(stderr, "%s: cvec_new: %s\n", __FUNCTION__, strerror(errno));
	goto catch;
    }
    /* for each lvalue, add cv */
    val = lvi = (struct lvalue *)lvec;
    i = 0;
    while ((void *)val < (void *)lvi+len) {
	/* The name must be a string */
	if (val->lv_type != CGV_STRING) {
	    clicon_log(LOG_WARNING, "%s: db entry should be string this is of type %d\n", 
		       __FUNCTION__, val->lv_type);
	    /* silently drop: see note, this could be a vector entry */
	    goto catch;
	}
	if ((name = strndup (val->lv_val, val->lv_len)) == NULL) {
	    clicon_err(OE_DB, errno , "%s: alloc name failed\n", __FUNCTION__);
	    goto catch;
	}
	unique = name[0] == '!';
	/* Jump to next lvalue */
	val = (struct lvalue *)((void *)val + hlen+val->lv_len);
	if (val >= lvi+len) {
	    clicon_err(OE_DB, errno , "%s: variable %s missing value in lvec\n",
		       __FUNCTION__, name);
	    goto catch;
	}
	/* add cv with name as name */
	cv = cvec_i(vr, i++);
	lv2cv(val, cv);
	if ((cv_name_set(cv, unique?name+1:name)) == NULL){
	    clicon_err(OE_DB, errno , "%s: alloc name failed\n", __FUNCTION__);
	    goto catch;
	}
	if (unique)
	    cv_flag_set(cv, V_UNIQUE);
	/* Jump to next lvalue */
	val = (struct lvalue *)((void *)val + hlen+val->lv_len);
	free(name);
	name = NULL;
    }
    retval = vr;
catch:
    if (retval == NULL && vr != NULL)
	cvec_free(vr);
    return retval;
}

/*
 * cvval2lv
 * Create the value part of lvalue from a cv. Ie type, name, etc is already handled.
 * XXX URLs not supported 
 */
static int
cvval2lv(cg_var *cv, struct lvalue *lv)
{
    int len = cv_len(cv);

    lv->lv_len = len;
    lv->lv_type = cv_type_get(cv);
    /* Special case: string is a pointer */
    if (cv_inline(lv->lv_type))
	memcpy(lv->lv_val, cv_value_get(cv), len); /* XXX */
    else
	/* XXX URLs not supported */
	memcpy(lv->lv_val, cv_string_get(cv), len); 
    return 0;
}

/*
 * cvec2lvec
 * return new lvec as malloced data
 * Note that a lvec is a vector of lvalue pairs.
 */
char *
cvec2lvec(cvec *vr, size_t *lveclen)
{
    cg_var        *cv = NULL;
    struct lvalue *lv = NULL;
    int            hlen; /* (fixed) header length */
    int            tlen; /* total length of lvec */
    int            len1, len2; 
    char          *lvec;
    char          *cv_name;

    lvec = NULL;
    *lveclen = 0;
    tlen = 0;
    hlen = (void*)lv->lv_val - (void*)lv;

    /* First compute total length */
    cv = NULL;
    while ((cv = cvec_each(vr, cv)) != NULL) {
	len1 = hlen + 1; 
	len1 = hlen + strlen(cv_name_get(cv)) + 1; /* first part contains name */
	if (cv_flag(cv, V_UNIQUE)) /* '!' */
	    len1++;
	len2 = hlen + cv_len(cv); /* second part contains value */
	tlen += len1 + len2;
    }
    if ((lvec = malloc(tlen)) == NULL) {
	clicon_err(OE_UNIX, errno, "%s malloc", __FUNCTION__);
	goto err;
    }
    memset(lvec, 0, tlen);
    /* Second, convert cligen variables to lvalues and encode in the lvec */
    cv = NULL;
    lv = (struct lvalue *)lvec;
    while ((cv = cvec_each(vr, cv)) != NULL) {
	cv_name = cv_name_get(cv);
	len1 = hlen + strlen(cv_name) + 1; /* first part contains name */
	if (cv_flag(cv, V_UNIQUE)) /* '!' */
	    len1++;

	/* Append variable name lvalue */
	lv->lv_type = CGV_STRING;
	lv->lv_len = len1-hlen;
	snprintf(lv->lv_val, len1-hlen, "%s%s", 
		 cv_flag(cv, V_UNIQUE)?"!":"", 
		 cv_name);
	lv = (struct lvalue *)(((char*)lv) + len1);

	/* Second part contains value: append from cv */
	len2 = hlen + cv_len(cv); 
	if (cvval2lv(cv, lv) < 0)
	    goto err;
	lv = (struct lvalue *)(((char*)lv) + len2);
    }
    assert((char*)lv-lvec == tlen);
    *lveclen = tlen;
    return lvec;
  err:
    if (lvec)
	free(lvec);
    return NULL;
}

/*
 * cv2lv
 * return value is malloced, and needs to be freed
 */
struct lvalue *
cv2lv (cg_var *cv)
{
  int len;
  int hlen;
  struct lvalue *lv = NULL;
  
  len = cv_len(cv);
  hlen = (void*)lv->lv_val - (void*)lv;
  if ((lv = (struct lvalue *)malloc(hlen+len)) == NULL) {
      clicon_err(OE_UNIX, errno, "malloc");
      return NULL;
  }

  if (cvval2lv(cv, lv) < 0){
      free(lv);
      return NULL;
  }
  return lv;
}


/*
 * lv2cv
 * Translate an lvalue to a cligen_variable
 * XXX URLs not supported
 */
int
lv2cv(struct lvalue *lv, cg_var *cgv)
{
    cv_type_set(cgv, lv->lv_type);
    cv_name_set(cgv, NULL);
    if (cv_inline(cv_type_get(cgv)))
	memcpy(cv_value_get(cgv), lv->lv_val, lv->lv_len);
    else 		
	if((cv_string_set(cgv, lv->lv_val)) == NULL){
	    clicon_err(OE_UNIX, errno, "strdup");
	    return -1;
	}
    return 0;
}

/*
 * lv2str
 * lvalue to string
 * Via a cgv, returns a malloced string that needs to be free:d. Or NULL.
 */
char *
lv2str(struct lvalue *lv)
{
    cg_var      *cv = NULL;
    char        *str = NULL;

    if ((cv = cv_new(CGV_STRING)) == NULL){
	clicon_err(OE_UNIX, errno, "cv_new");
	goto catch;
    }
    lv2cv(lv, cv);
    if ((str = cv2str_dup(cv)) == NULL)
	goto catch;
  catch:
    if (cv)
	cv_free(cv);
    return str;
}


/*
 * Given a basekey in spec-format, generate a regexp key to be
 * used for matching real database keys.
 */
char *
db_gen_rxkey(char *basekey, const char *label)
{
    int nvec;
    char **vec; 
    char *tmp;
    char *key = NULL;;
    
    if ((vec = clicon_strsplit (basekey, "[]", &nvec, __FUNCTION__))==NULL){
	clicon_err(OE_DB, errno, "%s: strsplit", __FUNCTION__);
	goto quit;
    }
    if ((tmp = clicon_strjoin(nvec, vec, "\\.[0-9]+", __FUNCTION__))==NULL){
	clicon_err(OE_DB, errno, "%s: join", __FUNCTION__);
	goto quit;
    }
    if  ((key = chunk_sprintf(label, "^%s$", tmp)) == NULL){
	clicon_err(OE_DB, errno, "%s: chunk_sprintf", __FUNCTION__);
	goto quit;    
    }

quit:
    unchunk_group(__FUNCTION__);
    return key;
}



/*
 * dbspec_last_unique_str
 * Given key, and its db_spec, return last unique variable in variable list of that key
 * Useful in db key lists (A.B.n $!a $!b $c $d) where last unique variable should 
 * determine index, in this example $!b.
 */
char *
dbspec_last_unique_str(struct db_spec *ds, cvec *setvars)
{
    cvec           *vr;
    cg_var         *cv;
    cg_var         *cvu = NULL; /* unique */
    cg_var         *cvc;

    vr = db_spec2cvec(ds);
    cv = NULL;
    while ((cv = cvec_each(vr, cv)))
	if (cv_flag(cv, V_UNIQUE))
	    cvu = cv;

    /* Get value of unique variable - value we should set/delete */
    if ((cvc = cvec_find(setvars, cv_name_get(cvu))) == NULL){
	clicon_log(LOG_ERR, "%s: No unique value for %s found in setvars\n",
		   __FUNCTION__, cv_name_get(cvu));
	return NULL;
    }
    
    return cv2str_dup(cvc);
}



/*
 * Same as cli_proto_change just with a cvec instead of lvec, and get the sock
 * from handle.
 * Utility function. COnsider moving to clicon_proto_client.c
 */
int
cli_proto_change_cvec(clicon_handle h, char *db, lv_op_t op,
		      char *key, cvec *cvv)
{
    char            *lvec = NULL;
    size_t           lvec_len;
    char            *spath;
    int              retval = -1;

    if ((spath = clicon_sock(h)) == NULL)
	goto done;
    if ((lvec = cvec2lvec(cvv, &lvec_len)) == NULL)
	goto done;
    retval = cli_proto_change(spath, db, op, key, lvec, lvec_len);
  done:
    if (lvec)
	free(lvec);
    return retval;
}
