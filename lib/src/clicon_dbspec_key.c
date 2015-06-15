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
 * Database specification
 * Syntax:
 * <line> ::= <key> <var>*
 * <var>  ::= $[!]<name>[:<type>]
 * Example: system.hostname a:string !b:number
 * Comment sign is '#'
 * The resulting parse-tree is in a linked list of db_spec:s
 * Each db_spec contains a key and a variable-headm which in turn contains
 * a list of variables 
 *
 * Translation between database specs
 *     dbspec_key                   yang_spec                     CLIgen parse_tree
 *  +-------------+    key2yang    +-------------+   yang2cli    +-------------+
 *  |             | -------------> |             | ------------> | cli         |
 *  |  A[].B !$a  |    yang2key    | list{key A;}|               | syntax      |
 *  +-------------+ <------------  +-------------+               +-------------+
 *        ^                             ^
 *        |db_spec_parse_file           |yang_parse
 *        |                             |
 *      <file>                        <file>
 */

#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#define __USE_GNU /* strverscmp */
#include <string.h>
#include <arpa/inet.h>
#include <regex.h>
#include <syslog.h>
#include <assert.h>
#include <sys/stat.h>
#include <netinet/in.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include "clicon_log.h"
#include "clicon_err.h"
#include "clicon_string.h"
#include "clicon_queue.h"
#include "clicon_hash.h"
#include "clicon_handle.h"
#include "clicon_dbspec_key.h"
#include "clicon_hash.h"
#include "clicon_lvalue.h"
#include "clicon_lvmap.h"
#include "clicon_chunk.h"
#include "clicon_yang.h"
#include "clicon_yang2key.h"
#include "clicon_options.h"
#include "clicon_dbutil.h"

dbspec_key *
db_spec_new(void)
{
  dbspec_key *ds;

  if ((ds = (dbspec_key *)malloc (sizeof (dbspec_key))) == NULL)
    return NULL;
  memset (ds, 0, sizeof (dbspec_key));
  return ds;
}

/*! Free a single db_spec struct */
static int
db_spec_free1(dbspec_key *ds)
{
    cvec_free(ds->ds_vec);
    assert(ds->ds_key);
    free(ds->ds_key); 
    if(ds->ds_index)
	hash_free(ds->ds_index);
    free(ds);
    return 0;
}

/*! Add db_spec element to end of list. Merge vars if same key
 * 
 * Note, ds may be freed.
 */
#if 0
 int
 db_spec_tailadd(dbspec_key **ds_list, dbspec_key *ds)
 {
    dbspec_key **dsp = ds_list;
     dbspec_key  *ds1;
     cg_var      *cv;
     cg_var      *cv2;
     int          retval = -1;
 
    while (*dsp){
        ds1 = *dsp;
       if (strcmp(ds1->ds_key, ds->ds_key) == 0){ /* same key? */
           cv = NULL;
           /* Go thru vars in the new key and see if exists in old */
           while ((cv = cvec_each(ds->ds_vec, cv)) != NULL) {
               if (cvec_find(ds1->ds_vec, cv_name_get(cv)) == NULL){
                   /* Add if not found */
                   if ((cv2 = cvec_add(ds1->ds_vec, cv_type_get(cv))) == NULL){
                       clicon_err(OE_UNIX, errno, "cvec_add");
                       goto done;
                   }
                   if (cv_cp(cv2, cv) != 0) {
                       clicon_err(OE_UNIX, errno, "cv_cp");
                       goto done;
                   }
                }
            }
       ds1->ds_vector = ds->ds_vector; /* broken */
            db_spec_free1(ds); /* already in list, remove OK */
            retval = 0;
            goto done; 

        }
       dsp = &(*dsp)->ds_next;
    }
    *dsp = ds;
     retval = 0;

   done:
     return retval;
 }
#else /* bennys optimization */
int
db_spec_tailadd(dbspec_key **ds_list, dbspec_key *ds)
{
    dbspec_key **dsp;
    dbspec_key  *ds1;
    cg_var      *cv;
    cg_var      *cv2;
    int          retval = -1;

    /*
     * If first entry, simply assign and create list hashed key index 
     */
    if (*ds_list == NULL) {
	*ds_list = ds;
	if ((ds->ds_index = hash_init()) == NULL)
	    return -1;
	/* Add tail pointer to index */
	ds->ds_tail = ds;
	/* Add new key to index */
	if (hash_add (ds->ds_index, ds->ds_key, &ds, sizeof(ds)) == NULL) {
	    hash_free(ds->ds_index);
	    return -1;
	}
	return 0;
    }

    /*
     * Not first entry; Try to match existing key/vars
     */
    assert((*ds_list)->ds_index);
    dsp = (dbspec_key **)hash_value((*ds_list)->ds_index, ds->ds_key, NULL);
    if (dsp) {
	ds1 = *dsp;
	cv = NULL;
	/* Go thru vars in the new key and see if exists in old */
	while ((cv = cvec_each(ds->ds_vec, cv)) != NULL) {
	    if (cvec_find(ds1->ds_vec, cv_name_get(cv)) == NULL){
		/* Add if not found */
		if ((cv2 = cvec_add(ds1->ds_vec, cv_type_get(cv))) == NULL){
		    clicon_err(OE_UNIX, errno, "cvec_add");
		    goto done;
		}
		if (cv_cp(cv2, cv) != 0) {
		    clicon_err(OE_UNIX, errno, "cv_cp");
		    goto done;
		}
	    }
	}
	ds1->ds_vector = ds->ds_vector; /* broken */
	db_spec_free1(ds); /* already in list, remove OK */
	retval = 0;
	goto done; 
    }
    
    /* Key not found; add to tail and update index */
    if (hash_add ((*ds_list)->ds_index, ds->ds_key, &ds, sizeof(ds)) == NULL)
	goto done;
    assert((*ds_list)->ds_tail);
    (*ds_list)->ds_tail->ds_next = ds;
    (*ds_list)->ds_tail = ds;
    retval = 0;

  done:
    return retval;
}
#endif

/*! Parse a single line of dbspec key syntax
 *
 * <line> ::= <key> <var>*
 * <var>  ::= $[!]<name>[:<type>]
 * <var>  ::= $[!]<name>[:<type>][=<val>]
 */
static int
dbspec_parse_line(char *line, int linenr, const char *filename, dbspec_key **list)
{
    int retval = -1;
    char  **vec;
    char   *var;
    char   *typeval;
    char   *type = NULL;
    char   *val = NULL;
    int     i;
    int     nvec;
    dbspec_key  *dbp;
    cvec            *vr;
    int              unique;
    cg_var          *newcv = NULL; 
    enum cv_type    cv_type;

    /* isue with trim of spaces ? */
    if ((vec = clicon_sepsplit(line, " \t", &nvec, __FUNCTION__)) == NULL)
	goto catch;
    if (nvec==0 || (nvec == 1 && !strlen(vec[0]))) // ignore empty
	goto catch;
    if ((dbp = db_spec_new()) == NULL)
	goto catch;
    if ((vr = cvec_new(0)) == NULL)
	goto catch;
    for (i=1; i<nvec; i++){
	unique = 0;
	if (!strlen(vec[i]))
	    continue;
	if (*vec[i] != '$'){ /* only variables accepted */
	    clicon_log(LOG_WARNING, "%s line:%d '%s' not variable (prepend with $)", 
		       filename, linenr, vec[i]);
	    continue;
	}
	vec[i]++;
	/* separate var into variable name and type+default */
	if (clicon_sep(vec[i], ":", __FUNCTION__, &var, &typeval) < 0)
	    goto catch;
	if (strlen(var) == 0){
	    clicon_log(LOG_WARNING, "db_spec_file line:%d '%s' no variable name", 
		    linenr, vec[i]);
	    continue;
	}
	/* separate typeval into type=default */
	type = NULL; val = NULL;
	if (typeval && clicon_sep(typeval, "=", __FUNCTION__, &type, &val) < 0)
	    goto catch;
	if (*var == '!'){
	    unique++;
	    var++;
	}
	if ((cv_type = cv_str2type(type?type:"string")) == CGV_ERR){
	    clicon_log(LOG_WARNING, "db_spec_file line:%d '%s' is not a cgv type", 
		    linenr, type);
	    continue;
	}
	if ((newcv = cvec_add(vr, cv_type)) == NULL) {
	    clicon_err(OE_UNIX, errno, "Allocating cligen object"); 
	    goto catch;
	}
	if (cv_name_set(newcv, var) == NULL) {
	    clicon_err(OE_UNIX, errno, "cv_name_set"); 
	    goto catch;
	}
	/* Default values */
	if (val != NULL){ 
	    if (cv_parse (val, newcv) < 0){
		clicon_log(LOG_WARNING, "db_spec_file line:%d '%s' malformed default value %s", 
			   linenr, val);
		goto catch;
	    }
	}
	else 
	    cv_flag_set(newcv, V_UNSET);

	if (unique)
	    cv_flag_set(newcv, V_UNIQUE);
    } /* for */
    /* Add this cvec to the spec */
    if ((dbp = db_spec_new()) == NULL)
	goto catch;
    dbp->ds_vec = vr;
    dbp->ds_key = strdup4(vec[0]);
    db_spec_tailadd(list, dbp); /* dbp may be freed */
    retval = 0;
  catch:
    unchunk_group(__FUNCTION__);
    return retval;
}

/*! Parse a db specification from text format to key-dbspec format.
 *
 * used internally in syntax-checking functions (such as key2spec_key()).
 * Free the returned parse-tree with db_spec_free()
 * (Only way to create a db_spec)
 * Return value:
 * db_spec - specification
 */
static dbspec_key *
db_spec_parse_file(const char *filename)
{
    char   *ptr;
    int     len;
    char   *start;
    int     linenr = 1;
    char    line[1024];
    dbspec_key *db_spec_list = NULL; 
    FILE   *f;
    if ((f = fopen(filename, "r")) == NULL){
	clicon_err(OE_UNIX, errno, "fopen(%s)", filename);	
	return NULL;
    }
    while (fgets(line, sizeof(line), f)) {
	ptr = start = (char*)line;
	while (*ptr != '\0'){ 	/* read line */
	    if (*ptr == '#'){ /* comment */
		*ptr = '\0';
		break;
	    }
	    if (*ptr == '\n'){
		*ptr = '\0';
		break;
	    }
	    ptr++;
	}
	len = ptr-start;
	if (len)
	    if (dbspec_parse_line(line, linenr, filename, &db_spec_list) < 0)
		goto catch;
	linenr++;
    }
  catch:
    fclose(f);

    return db_spec_list;
}


/*! Free a list of db_spec structs, ie a complete database specification */
int
db_spec_free(dbspec_key *db_spec_list)
{
    dbspec_key *db;

    while ((db = db_spec_list) != NULL){
	db_spec_list = db->ds_next;
	db_spec_free1(db);
    }
    return 0;
}

char *
db_spec2str(dbspec_key *db)
{
    char   *str;
    char   *retval = NULL;
    cvec   *vec;
    cg_var *cv;

    if ((str = chunk_sprintf(__FUNCTION__, "%s", db->ds_key)) == NULL)
	goto done;
    if ((vec = db_spec2cvec(db)) != NULL){
	cv = NULL;
	while ((cv = cvec_each(vec, cv))) {
	    if ((str = chunk_sprintf(__FUNCTION__, "%s $%s%s",  
				     str,
				     cv_flag(cv, V_UNIQUE)?"!":"",
				     cv_name_get(cv)
				     )) == NULL)
		goto done;
	}
    }
    retval = strdup(str);
  done:
    unchunk_group(__FUNCTION__);
    return retval;
    
}
/* 
 * db_spec_dump
 * debug, should be equiv to db_spec_file.txt 
 */
int
db_spec_dump(FILE *f, dbspec_key *dbl)
{
    dbspec_key *db;
    char           *str;
    int             retval = -1;

    for (db=dbl; db; db=db->ds_next){
	if ((str = db_spec2str(db)) == NULL)
	    goto done;
	fprintf(f, "%s", str);
	free(str);
#ifdef obsolete
	fprintf(f, "%s", db->ds_key);
	if ((vec = db_spec2cvec(db)) != NULL){
	    cv = NULL;
	    while ((cv = cvec_each(vec, cv))) {
		fprintf(f, " $%s%s:%s",  
			cv_flag(cv, V_UNIQUE)?"!":"",
			cv_name_get(cv),
			cv_type2str(cv_type_get(cv))
		    );
		if (!cv_flag(cv, V_UNSET)){
		    fprintf(f, "=");
		    cv_print(f, cv);
		}
	    }
	}
#endif
	fprintf(f, " \n");
    }
    retval = 0;
  done:
    return retval;
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
 * dbspec_key_match
 * key on the form A.4.B.4
 * skey on the form A[]B[]
 * return 0 on fail, 1 on sucess.
 * key:  a.0b
 * skey: a[]
 */
static int
dbspec_key_match(char *key, char *skey)
{
    int   i, j;
    char  k, s;
    int   vec;
    
    vec = 0;
    for (i=0, j=0; i<strlen(key) && j<strlen(skey); i++, j++){
	k = key[i];
	s = skey[j];
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
	return 1; /* ok */
    else
	return 0; /* fail */
}

/*
 * key2spec_key
 * Get db_spec struct
 * given a specific key, find a matching specification, (dbsepc key-style)
 * e.g. a.0 matches the db_spec corresponding to a[].
 * Input args:
 *  dbspec_list - db specification as list of keys
 *  key - key to find in dbspec
 */
dbspec_key *
key2spec_key(dbspec_key *dbspec_list, char *key)
{
    dbspec_key *db;
    int             ret;

    for (db=dbspec_list; db; db=db->ds_next){
	if ((ret = dbspec_key_match(key, db->ds_key)) < 0)
	    goto catch;
	if (ret)
	    break;
    } /* for */
    return db; // db_spec2vh(db);
  catch:
    return NULL;
}

/*
 * Sanity check on key and its variable list vh
 * Check against the database that:
 * 1. the key exists, 
 * 2. each variable exists
 * 3. the type of each variable match
 * 4. flags (unique) of each variable match
 * Sanity errors are logged
 * spec is the db_spec associated with key (not dbspec list)
 */
int
sanity_check_cvec(char *key, dbspec_key *spec, cvec *vec)
{
    cvec *vecs;
    cg_var *cv, *cvs;
    char *vkey;
    int errs = 0;

    vecs = db_spec2cvec(spec);
    cv = NULL;
    while ((cv = cvec_each(vec, cv))) {		
	vkey = cv_name_get(cv);
	if (vkey[0]=='!')
	    vkey++;
	if ((cvs = cvec_find(vecs, vkey)) == NULL){
	    clicon_log(LOG_WARNING, "%s: %s $%s not found in database spec (spec key:%s)\n",
		       __FUNCTION__, key, vkey, spec->ds_key);
	    errs++;
	    continue;
	}
	if (cv_type_get(cv) != cv_type_get(cvs)) {
	    clicon_log(LOG_WARNING, "%s: %s $%s: type (%s) does not match spec (%s)\n",
		       __FUNCTION__, key, cv_name_get(cv), 
		       cv_type2str(cv_type_get(cv)),
		       cv_type2str(cv_type_get(cvs)));
	    errs++;
	}
	if (cv_flag(cv, V_UNIQUE) != cv_flag(cvs, V_UNIQUE)){
	    clicon_log(LOG_WARNING, "%s: %s $%s: flag (%d) does not match spec (%d)\n",
		       __FUNCTION__, key, cv_name_get(cv),
		       cv_flag(cv, V_UNIQUE), cv_flag(cvs, V_UNIQUE));
	    errs++;
	}
    }
    return 0; /* ignore errors */

}

/*! Utility function for handling key parsing and translation to yang format
 * @param h          clicon handle
 * @param f          file to print to (if one of print options are enabled)
 * @param printspec  print database (KEY) specification as read from file
 * @param printalt   print alternate specification (YANG)
 */
int
dbspec_key_main(clicon_handle h, FILE *f, int printspec, int printalt)
{
    int             retval = -1;
    dbspec_key     *db_spec;
    char           *db_spec_file;
    struct stat     st;
    yang_spec      *yspec;

    /* Parse db specification */
    if ((db_spec_file = clicon_dbspec_file(h)) == NULL){
	clicon_err(OE_FATAL, errno, "No CLICON_DBSPEC_FILE given");
	goto done;
    }
    clicon_debug(1, "CLICON_DBSPEC_FILE=%s", db_spec_file);
    if (stat(db_spec_file, &st) < 0){
	clicon_err(OE_FATAL, errno, "CLICON_DBSPEC_FILE: %s not found", db_spec_file);
	goto done;
    }
    if ((db_spec = db_spec_parse_file(db_spec_file)) == NULL)
	goto done;
    if (printspec) 
	db_spec_dump(f, db_spec);
    clicon_dbspec_key_set(h, db_spec);	
    /* Translate to yang spec */
    if ((yspec = key2yang(db_spec)) == NULL)
	goto done;
    clicon_dbspec_yang_set(h, yspec);
    if (printalt)
	yang_print(f, (yang_node*)yspec, 0);
    retval = 0;
  done:
    return retval;
}
