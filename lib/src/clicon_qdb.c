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

 */

#ifdef HAVE_CONFIG_H
#include "clicon_config.h"
#endif



#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <limits.h>
#include <regex.h>
#include <sys/stat.h>
#include <sys/param.h>

#if defined(HAVE_DEPOT_H) || defined(HAVE_QDBM_DEPOT_H)
#ifdef HAVE_DEPOT_H
#include <depot.h> /* qdb api */
#else /* HAVE_QDBM_DEPOT_H */
#include <qdbm/depot.h> /* qdb api */
#endif 

/* clicon */
#include "clicon_log.h"
#include "clicon_err.h"
#include "clicon_queue.h"
#include "clicon_chunk.h"
#include "clicon_db.h" /* api defined here, there isno osr_qdb.h */

/*
 * db_init_mode
 * We could hardwire all databases to be in OSR_DB_DIR?
 */
static int 
db_init_mode(char *file, int omode)
{
    DEPOT *dp;
#if 0 /* Check in OSR? */
    struct stat sb;  

    if (stat(OSR_DB_DIR, &sb) < 0){               
        clicon_err(OE_DB, errno, "%s", OSR_DB_DIR);
	return -1; 
    }          
    if (!S_ISDIR(sb.st_mode)){
	clicon_err(OE_DB, 0, "db_init: %s not directory", file);
	return -1; 
    }
#endif
    /* Open database for writing */
    if ((dp = dpopen(file, omode | DP_OLCKNB, 0)) == NULL){
	clicon_err(OE_DB, 0, "db_init: dpopen(%s): %s", 
		file,
		dperrmsg(dpecode));
	return -1;
    }
    clicon_debug(1, "db_init(%s)", file);
    if (dpclose(dp) == 0){
	clicon_err(OE_DB, 0, "db_set: dpclose: %s", dperrmsg(dpecode));
	return -1;
    }
    return 0;
}

/*
 * db_init
 * Open database for reading and writing
 */
int 
db_init(char *file)
{
    return db_init_mode(file, DP_OWRITER | DP_OCREAT );
}

int 
db_set(char *file, char *key, void *data, size_t datalen)
{
    DEPOT *dp;

    /* Open database for writing */
    if ((dp = dpopen(file, DP_OWRITER | DP_OLCKNB, 0)) == NULL){
	clicon_err(OE_DB, 0, "db_set: dpopen(%s): %s", 
		file,
		dperrmsg(dpecode));
	return -1;
    }
    clicon_debug(2, "%s: db_put(%s, len:%d)", 
		file, key, (int)datalen);
    if (dpput(dp, key, -1, data, datalen, DP_DOVER) == 0){
	clicon_err(OE_DB, 0, "%s: db_set: dpput(%s, %d): %s", 
		file,
		key,
		datalen,
		dperrmsg(dpecode));
	dpclose(dp);
	return -1;
    }
    if (dpclose(dp) == 0){
	clicon_err(OE_DB, 0, "db_set: dpclose: %s", dperrmsg(dpecode));
	return -1;
    }
    return 0;
}

/*
 * db_get
 * returns: 
 *   0 if OK: value returned. If not found, zero string returned
 *   -1 on error   
 */
int 
db_get(char *file, char *key, void *data, size_t *datalen)
{
    DEPOT *dp;
    int len;

    /* Open database for readinf */
    if ((dp = dpopen(file, DP_OREADER | DP_OLCKNB, 0)) == NULL){
	clicon_err(OE_DB, dpecode, "%s: db_get(%s, %d): dpopen: %s", 
		file,
		key,
		datalen,
		dperrmsg(dpecode));
	return -1;
    }
    len = dpgetwb(dp, key, -1, 0, *datalen, data);
    if (len < 0){
	if (dpecode == DP_ENOITEM){
	    data = NULL;
	    *datalen = 0;
	}
	else{
	    clicon_err(OE_DB, 0, "db_get: dpgetwb: %s (%d)", 
		    dperrmsg(dpecode), dpecode);
	    dpclose(dp);
	    return -1;
	}
    }
    else
	*datalen = len;	
    clicon_debug(2, "db_get(%s, %s)=%s", file, key, (char*)data);
    if (dpclose(dp) == 0){
	clicon_err(OE_DB, 0, "db_get: dpclose: %s", dperrmsg(dpecode));
	return -1;
    }
    return 0;
}

/*
 * db_get_alloc
 * Similar to db_get but returns a malloced pointer to the data instead 
 * of copying data to pre-allocated buffer. This is necessary if the 
 * length of the data is not known when calling the function.
 * returns: 
 *   0 if OK: value returned. If not found, zero string returned
 *   -1 on error   
 * Note: *data needs to be freed after use.
 * Example:
 *  char             *lvec = NULL;
 *  size_t            len = 0;
 *  if (db_get-alloc(dbname, "a.0", &val, &vlen) == NULL)
 *     return -1;
 */
int 
db_get_alloc(char *file, char *key, void **data, size_t *datalen)
{
    DEPOT *dp;
    int len;

    /* Open database for writing */
    if ((dp = dpopen(file, DP_OREADER | DP_OLCKNB, 0)) == NULL){
	clicon_err(OE_DB, 0, "db_get_alloc: dpopen(%s): %s", 
		file,
		dperrmsg(dpecode));
	return -1;
    }
    if ((*data = dpget(dp, key, -1, 0, -1, &len)) == NULL){
	if (dpecode == DP_ENOITEM){
	    *datalen = 0;
	    *data = NULL;
	    len = 0;
	}
	else{
	    /* No entry vs error? */
	    clicon_err(OE_DB, 0, "db_get_alloc: dpgetwb: %s (%d)", 
		    dperrmsg(dpecode), dpecode);
	    dpclose(dp);
	    return -1;
	}
    }
    *datalen = len;
    if (dpclose(dp) == 0){
	clicon_err(OE_DB, 0, "db_get_alloc: dpclose: %s", dperrmsg(dpecode));
	return -1;
    }
    return 0;
}

/*
 * Delete database entry
 * Returns -1 on failure, 0 if key did not exist and 1 if successful.
 */
int 
db_del(char *file, char *key)
{
    int retval = 0;
    DEPOT *dp;

    /* Open database for writing */
    if ((dp = dpopen(file, DP_OWRITER | DP_OLCKNB, 0)) == NULL){
	clicon_err(OE_DB, 0, "db_del: dpopen(%s): %s", 
		file,
		dperrmsg(dpecode));
	return -1;
    }
    if (dpout(dp, key, -1)) {
        retval = 1;
    }
    if (dpclose(dp) == 0){
	clicon_err(OE_DB, 0, "db_del: dpclose: %s", dperrmsg(dpecode));
	return -1;
    }
    return retval;
}


static DEPOT *iterdp = NULL;
static regex_t iterre;

static void
db_iterend()
{
  if (iterdp) {
    dpclose(iterdp);
    iterdp = NULL;
    regfree(&iterre);
  }
}


static int
db_iterinit(char *file, char *regexp)
{
    int status;
    char errbuf[512];

    /* If already iterating, close existing */
    if (iterdp)
      db_iterend();

    if ((status = regcomp(&iterre, regexp, REG_EXTENDED)) != 0) {
        regerror(status, &iterre, errbuf, sizeof(errbuf));
	clicon_err(OE_DB, 0, "db_iterinit: regcomp: %s", errbuf);
	return -1;
    }

    /* Open database for reading */
    if ((iterdp = dpopen(file, DP_OREADER | DP_OLCKNB, 0)) == NULL){
        regfree(&iterre);
	clicon_err(OE_DB, 0, "db_iterinit: dpopen(%s): %s", 
		file,
		dperrmsg(dpecode));
	return -1;
    }

    if(dpiterinit(iterdp) == 0) {
	clicon_err(OE_DB, 0, "db_iterinit: dpiterinit: %s", dperrmsg(dpecode));
	db_iterend();
	return -1;
    }

    return 0;
}


static struct db_pair *
db_iternext(int noval, const char *label)
{
  int status;
  char *key;
  void *val = NULL;
  int vlen = 0;
  size_t nmatch = 1;
  regmatch_t pmatch[1];
  struct db_pair *pair;

  if (!iterdp) {
    clicon_err(OE_DB, 0, "db_iternext: Iteration not initialized");
    return NULL;
  }
  key = NULL;
  while((key = dpiternext(iterdp, NULL)) != NULL) {

    status = regexec(&iterre, key, nmatch, pmatch, 0);
    if (status != 0) { /* No match */
      free(key);
      continue;
    }

    if (noval == 0) {
	if((val = dpget(iterdp, key, -1, 0, -1, &vlen)) == NULL) {
	    clicon_log(OE_DB, "%s: dpget: %s", __FUNCTION__, dperrmsg(dpecode));
	    goto err;
	}
    }

    if ((pair = chunk(sizeof(*pair), label)) == NULL) {
	clicon_log(OE_DB, "%s: chunk: %s", __FUNCTION__, strerror(errno));
	goto err;
    }
    memset (pair, 0, sizeof(*pair));

    pair->dp_key = chunk_sprintf(label, "%s", key);
    pair->dp_matched = chunk_sprintf(label, "%.*s",
				     pmatch[0].rm_eo - pmatch[0].rm_so,
				     key + pmatch[0].rm_so);
    if (pair->dp_key == NULL || pair->dp_matched == NULL) {
	clicon_err(OE_DB, errno, "%s: chunk_sprintf", __FUNCTION__);
	goto err;
    }
    if (noval == 0) {
	if (vlen){
	    pair->dp_val = chunkdup (val, vlen, label);
	    if (pair->dp_val == NULL) {
		clicon_err(OE_DB, errno, "%s: chunkdup", __FUNCTION__);
		goto err;
	    }
	}
	pair->dp_vlen = vlen;
    }
    
   free(key);
    free(val);
    key = val = NULL;

//    if (debug)
//      fprintf(stderr, "%s: Key matched: %s\n", __FUNCTION__, key);

    return pair;
  }

  /* End of db. Fall through */

 err:
  db_iterend();
  if (key)
    free(key);
  if (val)
    free(val);
  return NULL;
}


int
db_regexp(char *file,
	  char *regexp, 
	  const char *label, 
	  struct db_pair **pairs,
	  int noval)
{
  int npairs;
  struct db_pair *newpairs;
  struct db_pair *pair;
  
  npairs = 0;
  *pairs = NULL;

  /* Initiate iterator */
  if (db_iterinit (file, regexp) < 0)
    return -1;


  /* Iterate through DB */
  while ((pair = db_iternext(noval, label))) {
    /* Resize resulting array */
    newpairs = rechunk(*pairs, (npairs + 1) * sizeof(struct db_pair), label);
    if (newpairs == NULL) {
      clicon_err(OE_DB, errno, "%s: rechunk", __FUNCTION__);
      goto quit;
    }

    /* Assign pair to new vector entry */
    memcpy(&newpairs[npairs], pair, sizeof(*pair));
    unchunk(pair);

    (*pairs) = newpairs;
    npairs++;
  }
  
  db_iterend();
  return npairs;

 quit:
  db_iterend();
  return -1;
}

/*
 * Sanitize regexp string. Escape '\' etc.
 */
char *
db_sanitize(char *rx, const char *label)
{
  char *new;
  char *k, *p, *s;

  k = chunk_sprintf(__FUNCTION__, "%s", "");
  p = rx;
  while((s = strstr(p, "\\"))) {
    if ((k = chunk_sprintf(__FUNCTION__, "%s%.*s\\\\", k, s-p, p)) == NULL)
      goto quit;
    p = s+1;
  }
  if ((k = chunk_strncat(k, p, strlen(p), __FUNCTION__)) == NULL)
    goto quit;

  new = (char *)chunkdup(k, strlen(k)+1, label);
  unchunk_group(__FUNCTION__);
  return new;

 quit:
  unchunk_group(__FUNCTION__);
  return NULL;
}


#endif /* DEPOT */

