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

 * XML database
 Playing around with xml-database. Which API is necessary?
        get(db, filter, &xml)
        put(db, merge|replace, xml)

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <fnmatch.h>
#include <stdint.h>
#include <assert.h>
#include <syslog.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include "clicon_err.h"
#include "clicon_log.h"
#include "clicon_queue.h"
#include "clicon_string.h"
#include "clicon_chunk.h"
#include "clicon_hash.h"
#include "clicon_handle.h"
#include "clicon_dbspec_key.h"
#include "clicon_lvalue.h"
#include "clicon_dbutil.h"
#include "clicon_qdb.h"
#include "clicon_yang.h"
#include "clicon_handle.h"
#include "clicon_xml.h"
#include "clicon_xsl.h"
#include "clicon_xml_parse.h"

/*
 * Variables
 */
#ifdef CLICON_XMLDB
int xmldb = 1;
#else
int xmldb = 0;
#endif

/*! Help function to append key values from an xml list to a cbuf */
static int
append_listkeys(cbuf *ckey, cxobj *xt, yang_stmt *ys)
{
    int        retval = -1;
    yang_stmt *ykey;
    cxobj     *xkey;
    cg_var    *cvi;
    cvec      *cvk = NULL; /* vector of index keys */
    char      *keyname;

    if ((ykey = yang_find((yang_node*)ys, Y_KEY, NULL)) == NULL){
	clicon_err(OE_XML, errno, "%s: List statement \"%s\" has no key", 
		   __FUNCTION__, ys->ys_argument);
	goto done;
    }
    /* The value is a list of keys: <key>[ <key>]*  */
    if ((cvk = yang_arg2cvec(ykey, " ")) == NULL)
	goto done;
    cvi = NULL;
    /* Iterate over individual keys  */
    while ((cvi = cvec_each(cvk, cvi)) != NULL) {
	keyname = cv_string_get(cvi);
	if ((xkey = xml_find(xt, keyname)) == NULL){
	    clicon_err(OE_XML, errno, "XML list node \"%s\" does not have key \"%s\" child",
		       xml_name(xt), keyname);
	    goto done;
	}
	cprintf(ckey, "/%s", xml_body(xkey));
    }
    retval = 0;
 done:
    if (cvk)
	cvec_free(cvk);
    return retval;
}

/*! Help function to create xml key values */
static int
create_keyvalues(cxobj *x, yang_stmt *y, char *name, char *arg, char *keyname,
		 cxobj **xcp)
{
    int        retval = -1;
    cbuf      *cpath = NULL;
    cxobj     *xb;
    cxobj     *xc = NULL;

    if ((cpath = cbuf_new()) == NULL)
	goto done;
    cprintf(cpath, "%s[%s=%s]", name, keyname, arg);
    /* Check if key node exists */
    if ((xc = xpath_first(x, cbuf_get(cpath)))==NULL){
	if ((xc = xml_find(x, name))==NULL)
	    if ((xc = xml_new(name, x)) == NULL)
		goto done;
	if ((xb = xml_new(keyname, xc)) == NULL)
	    goto done;
	if ((xb = xml_new("body", xb)) == NULL)
	    goto done;
	xml_type_set(xb, CX_BODY);
	xml_value_set(xb, arg);
    }
    retval = 0;
 done:
    *xcp = xc;
    if (cpath)
	cbuf_free(cpath);
    return retval;
}

static yang_stmt *
yang_find_topnode(yang_spec *ysp, char *argument)
{
    yang_stmt *ys = NULL;
    yang_stmt *yc = NULL;
    int i;

    for (i=0; i<ysp->yp_len; i++){
	ys = ysp->yp_stmt[i];
	if ((yc = yang_find_syntax((yang_node*)ys, argument)) != NULL)
	    return yc;
    }
    return NULL;
}

static int
get(char      *dbname,
    yang_spec *ys,
    char      *key,
    char      *val,
    cxobj     *xt)
{
    int         retval = -1;
    char      **vec;
    int         nvec;
    int         i;
    char       *name;
    yang_stmt  *y;
    cxobj      *x;
    cxobj      *xc;
    yang_stmt *ykey;
    cg_var    *cvi;
    cvec      *cvk = NULL; /* vector of index keys */
    char      *keyname;
    char      *arg;
    
    fprintf(stderr, "%s %s\n", key, val?val:"");
    x = xt;
    assert(key && key[0]=='/');
    if ((vec = clicon_strsplit(key, "/", &nvec, __FUNCTION__)) == NULL)
	goto done;
    /* Element 0 is NULL '/', 
       Element 1 is top symbol and needs to find subs in all modules:
       spec->module->syntaxnode
     */
    if (nvec < 2){
	clicon_err(OE_XML, 0, "Malformed key: %s", key);
	goto done;
    }
    name = vec[1];
    if ((y = yang_find_topnode(ys, name)) == NULL){
	clicon_err(OE_UNIX, errno, "No yang node found: %s", name);
	goto done;
    }
    i = 2;
    while (i<nvec){
	name = vec[i];
	if ((y = yang_find_syntax((yang_node*)y, name)) == NULL){
	    clicon_err(OE_UNIX, errno, "No yang node found: %s", name);
	    goto done;
	}
	if (y->ys_keyword == Y_LIST){
	    /* 
	     * If xml element is a list, then the next element is expected to be
	     * a key value. Check if this key value is already in the xml tree,
	     * oterwise create it.
	     */
	    if ((ykey = yang_find((yang_node*)y, Y_KEY, NULL)) == NULL){
		clicon_err(OE_XML, errno, "%s: List statement \"%s\" has no key", 
			   __FUNCTION__, y->ys_argument);
		goto done;
	    }
	    /* The value is a list of keys: <key>[ <key>]*  */
	    if ((cvk = yang_arg2cvec(ykey, " ")) == NULL)
		goto done;
	    cvi = NULL;
	    /* Iterate over individual keys  */
	    while ((cvi = cvec_each(cvk, cvi)) != NULL) {
		keyname = cv_string_get(cvi);
		i++;
		if (i>=nvec){
		    clicon_err(OE_XML, errno, "List %s without argument", name);
		    goto done;
		}
		arg = vec[i];
		if (create_keyvalues(x, 
				     y, 
				     name, 
				     arg, 
				     keyname, 
				     &xc) < 0)
		    goto done;
		x = xc;
	    }
	}
	else
	    if ((xc = xml_find(x, name))==NULL)
		if ((xc = xml_new(name, x)) == NULL)
		    goto done;
	x = xc;
	i++;
    }
    if (val && xml_body(x)==NULL){
	if ((x = xml_new("body", x)) == NULL)
	    goto done;
	xml_type_set(x, CX_BODY);
	xml_value_set(x, val);
    }
    retval = 0;
 done:
    unchunk_group(__FUNCTION__);  
    return retval;
}

/*! Query database using xpath 
 * @param[in]  dbname Name of database to search in (filename including dir path)
 * @param[in]  xpath  String with XPATH syntax (or NULL for all)
 * @param[out] vec    Vector of xml-trees. Vector must be free():d after use
 * @param[out] veclen Length of vector in return value
 * @retval     0      OK
 * @retval     -1     Error
 * @code
 *   cxobj **xv, *xn;
 *   int     i, xlen;
 *   if (xpath_vec(cxtop, "//symbol/foo", &xv, &xlen) < 0)
 *     err;
 *   for (i=0; i<xlen; i++){
 *         xn = xv[i];
 *         ...
 *   }
 *   free(xv);
 * @endcode
 */
int
xmldb_get(char      *dbname, 
	  yang_spec *ys,
	  char      *xpath,
	  cxobj   ***vec, 
	  size_t    *len)
{
    int             retval = -1;
    int             i;
    int             npairs;
    struct db_pair *pairs;
    cxobj          *xt;

    /* Read in complete database (this can be optimized) */
    if ((npairs = db_regexp(dbname, "", __FUNCTION__, &pairs, 0)) < 0)
	goto done;
    if ((xt = xml_new("clicon", NULL)) == NULL)
	goto done;
    if (1) /* debug */
	for (i = 0; i < npairs; i++) 
	    fprintf(stderr, "%s %s\n", pairs[i].dp_key, pairs[i].dp_val?pairs[i].dp_val:"");

    for (i = 0; i < npairs; i++) {
	if (get(dbname, 
		ys, 
		pairs[i].dp_key,
		pairs[i].dp_val,
		xt) < 0)
	    goto done;
    }

#if 0
    if (xpath_vec(xt, xpath, len) )
	goto done;
#endif
    clicon_xml2file(stdout, xt, 0, 1);
    retval = 0;
 done:
    if (xt)
	xml_free(xt);
    unchunk_group(__FUNCTION__);
    return retval;
}

static int
put(char       *dbname, 
    yang_stmt  *ys, 
    lv_op_t     op, 
    cxobj      *xt,
    const char *key0)
{
    int        retval = -1;
    cxobj     *x = NULL;
    char      *key;
    cbuf      *ckey = NULL;
    char      *body;
    yang_stmt *y;

    if ((ckey = cbuf_new()) == NULL)
	goto done;
    cprintf(ckey, "%s/%s", key0, xml_name(xt));
    if (ys->ys_keyword == Y_LIST){
	/* Note: can have many keys */
	if (append_listkeys(ckey, xt, ys) < 0)
	    goto done;
    }
    key = cbuf_get(ckey);
    /* XXX: if merge|add|delete */
    body = xml_body(xt);
    fprintf(stderr, "%s %s\n", key, body?body:"");
    /* Write to database, key and a vector of variables */
    if (db_set(dbname, key, body?body:NULL, body?strlen(body)+1:0) < 0)
	goto done;

    /* For every node, create a key with values */
    while ((x = xml_child_each(xt, x, CX_ELMNT)) != NULL){
	if ((y = yang_find_syntax((yang_node*)ys, xml_name(x))) == NULL){
	    clicon_err(OE_UNIX, errno, "No yang node found: %s", xml_name(x));
	    goto done;
	}
	if (put(dbname, y, op, x, key) < 0)
	    goto done;
    }
    retval = 0;
 done:
    if (ckey)
	cbuf_free(ckey);
    return retval;
}

/*! Add data to database
 * @param[in]  dbname Name of database to search in (filename including dir path)
 * @param[in]  op     LV_DELETE, LV_SET, LV_MERGE
 * @param[in]  xt     xml-tree.
 * @retval     0      OK
 * @retval     -1     Error
 */
int
xmldb_put(char      *dbname, 
	  yang_spec *ys, 
	  lv_op_t    op, 
	  cxobj     *xt)
{
    int        retval = -1;
    cxobj     *x = NULL;
    yang_stmt *y;

    while ((x = xml_child_each(xt, x, CX_ELMNT)) != NULL){
	if ((y = yang_find_topnode(ys, xml_name(x))) == NULL){
	    clicon_err(OE_UNIX, errno, "No yang node found: %s", xml_name(x));
	    goto done;
	}
    	if (put(dbname, y, op, x, "") < 0)
    	    goto done;
    }
    retval = 0;
 done:
    return retval;
}

#if 1 /* Test program */
/*
 * Turn this on to get an xpath test program 
 * Usage: clicon_xpath [<xpath>] 
 * read xml from input
 * Example compile:
 gcc -g -o xmldb -I. -I../clicon ./clicon_xmldb.c -lclicon -lcligen
*/

static int
usage(char *argv0)
{
    fprintf(stderr, "usage:\n%s\tget <db> <yangdir> <yangmod> [<xpath>]\t\txml on stdin\n", argv0);
    fprintf(stderr, "\tput <db> <yangdir> <yangmod> set|merge|delete\txml to stdout\n");
    exit(0);
}

int
main(int argc, char **argv)
{
    int         i;
    cxobj     **vec;
    cxobj      *xt;
    cxobj      *xn;
    char       *xpath;
    size_t      len = 0;
    lv_op_t     op;
    char       *cmd;
    char       *db;
    char       *yangdir;
    char       *yangmod;
    yang_spec  *ys = NULL;
    clicon_handle h;

    if ((h = clicon_handle_init()) == NULL)
	goto done;
    clicon_log_init("xmldb", LOG_DEBUG, CLICON_LOG_STDERR);
    if (argc < 4){
	usage(argv[0]);
	goto done;
    }
    cmd = argv[1];
    db = argv[2];
    yangdir = argv[3];
    yangmod = argv[4];
    db_init(db);
    if ((ys = yspec_new()) == NULL)
	goto done;
    if (yang_parse(h, yangdir, yangmod, NULL, ys) < 0)
	goto done;
    if (strcmp(cmd, "get")==0){
	if (argc < 5)
	    usage(argv[0]);
	xpath = argc>5?argv[5]:NULL;
	if (xmldb_get(db, ys, xpath, &vec, &len) < 0)
	    goto done;
	for (i=0; i<len; i++){
	    xn = vec[i];
	    fprintf(stdout, "[%d]:\n", i);
	    clicon_xml2file(stdout, xn, 0, 1);	
	}
    }
    else
    if (strcmp(cmd, "put")==0){
	if (argc != 6)
	    usage(argv[0]);
	if (clicon_xml_parse_file(0, &xt, "</clicon>") < 0)
	    goto done;
	xn = xml_child_i(xt, 0);
	xml_prune(xt, xn, 0); /* kludge to remove top-level tag (eg top/clicon) */
	xml_parent_set(xn, NULL);
	xml_free(xt);
	if (strcmp(argv[5], "set") == 0)
	    op = LV_SET;
	else 	
	    if (strcmp(argv[4], "merge") == 0)
	    op = LV_MERGE;
	else 	if (strcmp(argv[5], "delete") == 0)
	    op = LV_DELETE;
	else
	    usage(argv[0]);
	if (xmldb_put(db, ys, op, xn) < 0)
	    goto done;
    }
    else
	usage(argv[0]);
    printf("\n");
 done:
    return 0;
}

#endif /* Test program */
