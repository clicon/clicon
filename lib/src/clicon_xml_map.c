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
 * XML code
 */
#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <syslog.h>
#include <fcntl.h>
#include <netinet/in.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */

#include "clicon_string.h"
#include "clicon_queue.h"
#include "clicon_hash.h"
#include "clicon_db.h"
#include "clicon_chunk.h"
#include "clicon_handle.h"
#include "clicon_dbspec_key.h"
#include "clicon_yang.h"
#include "clicon_options.h"
#include "clicon_lvalue.h"
#include "clicon_dbutil.h"
#include "clicon_lvmap.h"
#include "clicon_xml.h"
#include "clicon_xsl.h"
#include "clicon_log.h"
#include "clicon_err.h"
#include "clicon_xml.h"

/*! Create sub-elements for every variable in skiplist
 * The skipvr list are the 'unique' variables of vectors
 * that should already have been added.
 */
static int
var2xml_all(cxobj *xnp, cvec *vr, cvec *skipvr)
{
    cxobj    *xn;
    cxobj    *xnb;
    cg_var   *cv = NULL;
    char     *vname;
    char     *str;

    /* Print/Calculate varible format string if variable exist */
    cv = NULL;
    while ((cv = cvec_each(vr, cv))) {
	vname = cv_name_get(cv);
	if (cvec_find(skipvr, vname))
	    continue; /* skip the ones in skiplist */
	/* create a parse-node here */
	if ((xn = xml_new(vname, xnp)) == NULL)
	    goto catch;
	if ((str = cv2str_dup(cv)) == NULL)
	    goto catch;
	if ((xnb = xml_new("body", xn)) == NULL) {
	    free(str);
	    goto catch;
	}
	xml_type_set(xnb, CX_BODY);
	xml_value_set(xnb, str);
	free(str);
    } /* if var_find */
  catch:
    return 0;
}


/*
 * XXX: Is this really necessary?
 */
static int
db2xml_sort (const void *p1, const void *p2)
{
    char *ptr1, *ptr2;
    char *s1 = ((struct db_pair *)p1)->dp_key;
    char *s2 = ((struct db_pair *)p2)->dp_key;
    
    ptr1 = s1 + strlen(s1)-1;
    while (ptr1 > s1 && isdigit(*ptr1))
      ptr1--;
    if (*ptr1 != '.')
	goto regex;
    ptr1++;

    ptr2 = s2 + strlen(s2)-1;
    while (ptr2 > s2 && isdigit(*ptr2))
      ptr2--;
    if (*ptr2 != '.')
	goto regex;
    ptr2++;

    if(ptr1-s1 == ptr2-s2 && strncmp(s1, s2, ptr1-s1) == 0)
	return lvmap_get_dbvector_sort(p1, p2);
    
regex:
    return lvmap_get_dbregex_sort (p1, p2);
}


/* 
   Optimization of xpath_first(xn, "/node[key=str]")

   xn
   | \ 0..N, where N can be very large
   node "CreateSender..."
   |
   key "SenderName"
   |
   body value== str
*/
static cxobj *
xml_xfind_vec(cxobj *xn, char *node, cvec *cvv)
{
    cxobj  *xnode;
    cxobj  *xkey;
    cxobj  *xbody;
    cg_var *cv;
    int     found = 0;

    xnode = NULL;
    if (xn == NULL)
	return 0;
    while ((xnode = xml_child_each(xn, xnode, CX_ELMNT)) != NULL) {
	if (strcmp(node, xml_name(xnode)))
	    continue;
	xkey = NULL;
	/* Must match all variable and values in cvv */
	cv = NULL;
	while ((cv = cvec_each(cvv, cv)) != NULL){
	    found = 0;
	    while ((xkey = xml_child_each(xnode, xkey, CX_ELMNT)) != NULL) {
		if (strcmp(cv_name_get(cv), xml_name(xkey)))
		    continue;
		xbody = NULL;
		while ((xbody = xml_child_each(xkey, xbody, CX_BODY)) != NULL) {
		    if (strcmp(cv_string_get(cv), xml_value(xbody)))
			continue;
		    found++;
		    break;
		}
		break; /* Assume only one matching key */
	    }
	    if (!found) /* this cv not matched */
		break;
	} /* cv */
	if (found)
	    return xnode;
    }
    //  done:
    return NULL;
}

/*
 * dbkey2xml
 * key is a name of a unique variable, we look it up in variable list vh
 * and then find the object it corresponds to in xml tree xn.
 * @param[out]   xnt

 * XXX: A[] $!a; A[].B[] $!a $!b should be shown as:
 <Group>
  <GroupName>G</GroupName>
  <Node>
    <NodeName>N</NodeName>
    <Port>4242</Port>
  </Node>
</Group>

Now it is shown with this appended:

<Group>
  <GroupName>G</GroupName>
</Group>

 */
int
dbkey2xml(dbspec_key *db_spec, 
	  cxobj      *xnt, 
	  char       *key, 
	  char       *val, 
	  int         vlen)
{
    cxobj      *xnp;   /* parent */
    cxobj      *xn = NULL;
    cxobj      *xb;
    cxobj      *xv;
    char       *subkey;
    dbspec_key *subspec;
    cvec       *vr;
    cvec       *subvr;
    cg_var     *vs;
    cg_var     *v;
    char       *vname;
    cg_var     *ncv;
    char      **vec;
    int         nvec;
    int         isvec;
    int         n;
    char       *str;
    cvec       *uvr = NULL; /* unique var set */
    cvec       *uvr0 = NULL; /* tmp unique var set */
    int         prevspec;

    if (debug > 1)
	fprintf(stderr, "%s: key:%s\n", __FUNCTION__, key);
    xnp = xnt;
    if (key_isvector_n(key) || key_iskeycontent(key))
	return 0;
    if ((vec = clicon_strsplit(key, ".", &nvec, __FUNCTION__)) == NULL)
	goto catch;
    if ((vr = lvec2cvec(val, vlen)) == NULL)
	goto catch;
    if ((uvr = cvec_new(0)) == NULL){
	clicon_err(OE_UNIX, errno, "cvec_new");	
	goto catch;
    }
    prevspec = 1;
    /* 
     * semantics of prevspec:
     * It is 1 initially.
     * If No spec is found (eg A when A[]) it set to 0.
     * else if it is a vector it is set, or if it is not set previosly.
     * If prevspec is not set and a spec is not found, then an xml-node is 
     * 
     */
    for (n=0; n<nvec; n++){
	if ((subkey = clicon_strjoin(n+1, vec, ".", __FUNCTION__)) == NULL)
	    goto catch;
	/* a vector subkey may not have a spec: spec: a.b[], then a.b has
	   no spec */
	if ((subspec = key2spec_key(db_spec, subkey)) == NULL){
	    if (!prevspec){
		if  ((xn = xml_find(xnp, vec[n-1])) == NULL)
		    if ((xn = xml_new(vec[n-1], xnp)) == NULL)
			goto catch;
		xnp = xn;
	    }
	    prevspec = 0;
	    continue;
	}
	if (debug>1)
	    fprintf(stderr, "%s: subkey:%s\n", __FUNCTION__, subkey);
	isvec = key_isvector(subspec->ds_key);
	if (debug>1)
	    fprintf(stderr, "%s: isvec:%d\n", 
		    __FUNCTION__, isvec);
	if (isvec){ /* subkey is vector, on the form a.b.<n> */
	    prevspec++;
	    /* find unique variables from spec */
	    subvr = db_spec2cvec(subspec);
	    /* uvr0 is the list of unique variables for this key */
	    if ((uvr0 = cvec_new(0)) == NULL){
		clicon_err(OE_UNIX, errno, "cvec_new");	
		goto catch;
	    }
	    vs = NULL;
	    while ((vs = cvec_each(subvr, vs)))
		if (cv_flag(vs, V_UNIQUE)){
		    vname = cv_name_get(vs);
		    if (cvec_find(uvr, vname) == NULL){
			/* Add to unique vars vec */
			if ((ncv = cvec_add(uvr0, CGV_STRING)) == NULL)
			    goto catch;
			if (cv_name_set(ncv, vname) == NULL)
			    goto catch;
			//cv_flag_set(ncv, V_UNSET);
			/* get key's value of v (actual data, not the vs spec) */
			if ((v = cvec_find(vr, vname)) == NULL)
			    continue; /* bad spec */
			if ((str = cv2str_dup(v)) == NULL)
			    continue; /* bad spec */
			cv_string_set(ncv, str);
			free(str);
		    }
		}
	    if (cvec_len(uvr0) == 0){ /* bad spec */
		clicon_log(LOG_WARNING, "%s: list has no unique variable %s", 
			   __FUNCTION__, subkey);
		continue;
	    }
	    /* Copy local vnr0 to stacked vnr unique variables */
	    vs = NULL;
	    while ((vs = cvec_each(uvr0, vs)))
		cvec_add_cv(uvr, vs);
	    /* If node found, replace xnp and traverse one step deeper */
	    if  ((xn = xml_xfind_vec(xnp, vec[n-1], uvr0)) != NULL){
		xnp = xn;
		continue;
	    }
	    assert(n>0);
	    if ((xn = xml_new(vec[n-1], xnp)) == NULL)
		goto catch;
	    vs = NULL;
	    while ((vs = cvec_each(uvr0, vs))){
		vname = cv_name_get(vs); /* Cache name of the unique variable */
		str = cv_string_get(vs);
		if (debug>1)
		    fprintf(stderr, "%s: create <%s><%s>%s\n", 
			    __FUNCTION__, vec[n-1], vname, str);
		if ((xv = xml_new(vname, xn)) == NULL){
		    goto catch;
		}
		xml_index_set(xv, xml_index(xv)+1);
		if ((xb = xml_new("body", xv)) == NULL){
		    goto catch;
		}
		xml_type_set(xb, CX_BODY);
		xml_value_set(xb, str);
	    }
	    cvec_free(uvr0);
	    uvr0 = NULL;
	} /* vector */
	else{
	    if (!prevspec){
		prevspec++;
		if  ((xn = xml_find(xnp, vec[n-1])) == NULL)
		    if ((xn = xml_new(vec[n-1], xnp)) == NULL)
			goto catch;
		xnp = xn;
	    }
	    if  ((xn = xml_find(xnp, vec[n])) == NULL){
		if (debug>1)
		    fprintf(stderr, "%s: create <%s> parent:%s\n", 
			    __FUNCTION__, vec[n], xml_name(xnp));
		if ((xn = xml_new(vec[n], xnp)) == NULL)
		    goto catch;
	    }
	}
	xnp = xn;
    } /* for */
    if (var2xml_all(xn, vr, uvr) < 0)
	goto catch;
    if (uvr) /* clear unique var set */
	cvec_free(uvr);
    if (uvr0) /* clear unique var set */
	cvec_free(uvr0);
    cvec_free (vr);
    vr = NULL;
    unchunk_group(__FUNCTION__);  
    return 0;
  catch:
    if (uvr) /* clear unique var set */
	cvec_free(uvr);
    if (uvr0) /* clear unique var set */
	cvec_free(uvr0);
    unchunk_group(__FUNCTION__);  
    return -1;
}

/*
 * dbpairs2xml
 * Given a list of key/val pairs, return a xml parse-tree.
 * Sort them.
 * Build a parse-tree in xml, and return that.
 */
static int
dbpairs2xml(struct db_pair *pairs, 
	    int             npairs, 
	    dbspec_key     *db_spec, 
	    cxobj          *xnt)
{
    int              i;
    int retval = -1;

    /* XXX: Why do we need to sort the pairs ? */
    if(0)    qsort (pairs, npairs, sizeof (struct db_pair), db2xml_sort);
    for (i=0; i<npairs; i++) {
	if (dbkey2xml(db_spec, 
		      xnt, 
		      pairs[i].dp_key, 
		      pairs[i].dp_val, 
		      pairs[i].dp_vlen) < 0)
	    goto catch;
    } /* for pairs */
    retval = 0;
  catch:
    unchunk_group(__FUNCTION__);  
    return retval;
} /* dbpairs2xml */

/*! Given a database and key regex go through all keys and return an xml parse-tree.
 *
 * @param[in]  dbname    Name of database to check
 * @param[in]  dbspec    Database specification in key format
 * @param[in]  key_regex Reg-exp of database keys. "^.*$" is all keys in database
 * @param[in]  toptag    The XML tree returned will have this top XML tag
 * retval      xt        XML parse tree, free with xml_free()
 * retval      NULL      on error
 *
 * dont work:
 * system.hostname (only hostname)
 * inet.address (only hostname)
 */
cxobj *
db2xml_key(char       *dbname, 
	   dbspec_key *db_spec, 
	   char       *key_regex,
	   char       *toptag)
{
    struct db_pair   *pairs;
    int               npairs;
    cxobj            *xt;

    if (key_regex == NULL)
	key_regex = "^.*$";
    if ((xt = xml_new(toptag, NULL)) == NULL)
	goto catch;
    if ((npairs = db_regexp(dbname, key_regex, __FUNCTION__, &pairs, 0)) < 0)
	goto catch;
    if (dbpairs2xml(pairs, npairs, db_spec, xt) < 0)
	goto catch;
    unchunk_group(__FUNCTION__);  
    return xt;
  catch:
    if (xt)
	xml_free(xt);
    unchunk_group(__FUNCTION__);  
    return NULL;
}

/*! Given a database go through all keys and return an xml parse-tree. Consider remove */
cxobj *
db2xml(char       *dbname, 
       dbspec_key *db_spec, 
       char       *toptag)
{
    return db2xml_key(dbname, db_spec, "^.*$", toptag);
}

/*! Given a database and a key in that database, return an xml parsetree.
 * 
 * @param[out]   xtop
 */
int
key2xml(char *key, char *dbname, dbspec_key *db_spec, cxobj *xtop)
{
    char             *lvec = NULL;
    size_t            lvlen;
    int               retval = -1;

    if (db_get_alloc(dbname, key, (void*)&lvec, &lvlen) < 0)
	goto catch;
    if (dbkey2xml(db_spec, xtop, key, lvec, lvlen) < 0)
	goto catch;
    retval = 0;
  catch:
    if (lvec)
	free(lvec);
    return retval;
}



/*
 * A node is a leaf if it contains a body.
 */
static cxobj *
leaf(cxobj *xn)
{
    cxobj *xc = NULL;

    while ((xc = xml_child_each(xn, xc, CX_BODY)) != NULL)
	break;
    return xc;
}

/*!
 * @param  xn       xml-parse tree of a 'superleaf', ie parent to leafs
 * @param  dbspex   Database specification 
 * @param  dbname   Name of a database we are creating
 * @param  key      A database key, example ipv4.domain
 * @param  spec     A database spec entry.
 * @param  uv       Unique (key) variable vector
 */
static int
xml2db_transform_key(cxobj       *xn, 
		     dbspec_key  *dbspec, 
		     char        *dbname, 
		     int          isvector,
		     char       **key, 
		     dbspec_key  *spec,
		     cvec        *uv)
{
    cxobj  *xc;          /* xml parse-tree child (leaf) */
    cxobj  *xb;          /* xml body (text) */
    cvec             *vhds;          /* spec variable list */
    cg_var           *v;           /* single variable */
    int               retval = -1; 
    cvec             *vh1 = NULL;           
    cg_var           *cv;
    int               index;
    int               len;
    char             *k;
    int               matched = 0;

    /* vh1 is list of variables, first unique, then rest */
    if ((vh1 = cvec_new(0)) == NULL) {
	clicon_err(OE_UNIX, errno, "cvec_new");
	goto catch;
    }
    /* 
     * First add unique variables
     */
    v = NULL;
    while ((v = cvec_each(uv, v))){
	if((cv = cvec_add_cv(vh1, v)) == NULL) {
	    clicon_err(OE_UNIX, errno, "cvec_add_cv");
	    goto catch;
	}
    }
    /* 
     * Then add variables found in XML and check with dbspec
     */
    vhds = db_spec2cvec(spec);
    xc = NULL;
    while ((xc = xml_child_each(xn, xc, CX_ELMNT)) != NULL){
	xb = NULL;
	while ((xb = xml_child_each(xc, xb, CX_BODY)) != NULL){
	    /* unique cv already added? */
	    if (spec->ds_vector || (cv = cvec_find(vh1, xml_name(xc))) == NULL){ 
		if ((v = cvec_find(vhds, xml_name(xc))) == NULL)
		    continue;
		if ((cv = cvec_add_cv(vh1, v)) == NULL){
		    clicon_err(OE_UNIX, errno, "cvec_add");
		    goto catch;
		}
	    }
	    cv_parse(xml_value(xb), cv);
	    break; /* one body */
	} /* for body */
    }
    if (isvector){
	if ((index = db_lv_vec_find(dbspec, dbname, *key, vh1, &matched)) < 0)
	    goto catch;
	assert(index>=0);
	k = strdup(*key);
	len  = strlen(k)+1+index/10+1+1; /* .n */
	free(*key);
	if ((*key = malloc(len+3)) == NULL) /* bit-stuff to 32-bit */
	    goto catch;
	snprintf(*key, len, "%s.%u", k, index);
	free(k);
	if (db_lv_set(spec, dbname, *key, vh1, LV_SET) < 0)
	    goto catch;
#if 0 /* db_lv_vec_set() is inlined above for optimization reasons. XXX: Maybe can
       reinstall when optinmization finalized? */
	if (db_lv_vec_set(dbspec, dbname, k, vh1, LV_SET, &index) < 0)
	    goto catch;
#endif
    }
    else 
	if (db_lv_set(spec, dbname, *key, vh1, LV_SET) < 0)
	    goto catch;
    retval = 0;
  catch:
    if (vh1)
	cvec_free(vh1);
    unchunk_group(__FUNCTION__);
    return retval;
}

/*
 * key2spec
 * given key and key-prefix, return spec if exact found.
 * if not, there may be a partial match, ie continue parsing.
 * key is malloced, needs free after use
 */
static int
key2spec(char            *name, 
	 char            *key0, 
	 dbspec_key  *db_spec, 
	 dbspec_key **specp, 
	 int             *partial,
	 char           **key)
{
    char            *vkey; /* vector key */
    dbspec_key  *ds;
    int              retval = -1;
    int              len;

    *partial = 0;
    len = strlen(name);
    if (key0){
	len += strlen(key0) + 1;
	if ((*key = malloc(len+1)) == NULL){
	    clicon_err(OE_UNIX, errno, "malloc");
	    goto done;
	}
	snprintf(*key, len+1, "%s.%s", key0, name);
    }
    else{
	*key = strdup(name);
    }
    if ((vkey = chunk_sprintf(__FUNCTION__, "%s[]", *key)) == NULL)
	goto done;

    /* Try match key or key[] with spec */ 
    for (ds=db_spec; ds; ds=ds->ds_next){
	if (strcmp(*key, ds->ds_key) == 0)
	    break;
	if (strcmp(vkey, ds->ds_key) == 0){
	    free(*key);
	    if ((*key = strdup(vkey)) == NULL){
		clicon_err(OE_UNIX, errno, "strdup");
		goto done;
	    }
	    break;
	}
	if (strncmp(*key, ds->ds_key, len) == 0)
	    (*partial)++;
    }
    if (ds != NULL)
	*specp = ds;
    retval = 0;
  done:
    unchunk_group(__FUNCTION__);
    return retval;
}

/*
 * xml2db_1
 * XXX: We have a problem here with decimal64. the xml parsing is taking its info
 * from keyspec, but there is no fraction-digits there. Can we change this code to take the 
 * spec from yang instead or as a complimentary?
 */
static int
xml2db_1(cxobj          *xn, 
	 dbspec_key     *dbspec, 
	 char           *dbname, 
	 cvec           *uv,
	 char           *basekey0,     /* on format A[].B[] */
	 char           *key0
    )         /* on format A.2.B.3 */
{
    int               retval = -1;
    int               superleaf;
    cxobj            *xc;
    char             *basekey = NULL;
    char             *key = NULL;
//    char             *k;
    dbspec_key  *spec = NULL;
    cg_var           *v = NULL;
    char             *bstr;
    int               uniquevars = 0;
    int               partial = 0;
    cg_var           *cv; 
    int               len;

    /*
     * Identify what is called a 'superleaf'. This is an XML node with element
     * children which in turn do not have sub-element.
     * Example: x is a superleaf: <x><a>text</a><b/><c attr="foo/></x>
     */
    superleaf = 0;
    xc = NULL;

    /*
     * return matching database specification and specification key.
     * or a partial match, eg if[].inet would partially match if[].inet.address[]
     */
    if (key2spec(xml_name(xn), basekey0, dbspec, &spec, 
		 &partial, &basekey) < 0)
	goto catch;
    /* actual key */
    if (key0){
	len = strlen(key0) + 1 + strlen(xml_name(xn));
	if ((key = malloc(len+1)) == NULL)
	    goto catch;
	snprintf(key, len+1, "%s.%s", key0, xml_name(xn));
    }
    else
	if ((key = strdup4(xml_name(xn))) == NULL)
	    goto catch;

    if (spec == NULL){
	if (partial)
	    goto loop;
	retval = 0;
	goto catch;
    }
    /*
     * if this is a vector we can get its unique vars and make it
     * so we should transform it.
     * Otherwise we should transform if it has a proper spec
     * Actually maybe that is always when we should tranform it?
     */
    if (key_isvector(basekey)){
	v = NULL; 		
	while ((v = cvec_each(db_spec2cvec(spec), v)))
	    if (cv_flag(v, V_UNIQUE) &&
		!cvec_find(uv, cv_name_get(v))){
		/* push uniquevars */
		if ((bstr = xml_find_body(xn, cv_name_get(v))) == NULL){
		    clicon_log(LOG_WARNING, "%s: xml should contain unique variable %s", 
			       __FUNCTION__, cv_name_get(v));
		    goto catch; /* bad xml */
		}
		if ((cv = cvec_add(uv, cv_type_get(v))) == NULL){
		    clicon_err(OE_UNIX, errno, "cvec_add");
		    goto catch;
		}
		if (cv_name_set(cv, cv_name_get(v)) == NULL)
		    goto catch;
		cv_flag_set(cv, cv_flag(v, 0xff));
		cv_parse(bstr, cv);
		uniquevars++;
	    }
	if (uniquevars == 0) /* bad spec */
	    clicon_log(LOG_WARNING, "%s: xml should contain unique variable %s", 
		       __FUNCTION__, xml_name(xn));
#ifdef notanymore
	if (uniquevars > 1) /* bad spec */
	    clicon_log(LOG_WARNING, "%s: xml should contain exactly one new unique variable %s", 
		       __FUNCTION__, xml_name(xn));
#endif
    }
    /* At least one sub is a leaf, then mark this node as superleaf
       and transform to DB */

    if (!leaf(xn))
	while ((xc = xml_child_each(xn, xc, CX_ELMNT)) != NULL)
	    if (leaf(xc)){
		superleaf++;
		break;
	    }
    /* 
     */
    if (superleaf){
	if ((xml2db_transform_key(xn, dbspec, dbname, 
				  key_isvector(basekey), 
				  &key, spec, uv)) < 0)
	    goto catch;
    }
  loop:
    xc = NULL;
    while ((xc = xml_child_each(xn, xc, CX_ELMNT)) != NULL)
	if (!leaf(xc)){
	    /* XXX: Se skillnad mellan basekey och faktisk key. */
	    if (xml2db_1(xc, dbspec, dbname, uv, basekey, key) < 0)
		goto catch;
	}
    retval = 0;
  catch: /* note : no unchunk here: recursion */
    /* pop uniquevars */
    while (uniquevars--){
	if (cvec_len(uv) > 0){
	    cv = cvec_i(uv, cvec_len(uv)-1);
	    cv_reset(cv);
	    cvec_del(uv, cv);
	}
    }
    if (key)
	free(key);
    if (basekey)
	free(basekey);
    return retval;
}


/*
 * xml2db
 * Map an xml parse-tree to a database
 */
int 
xml2db(cxobj *xt, dbspec_key *dbspec, char *dbname)
{
    cxobj *x = NULL;
    int retval = 0;
    cvec *uv;

    /* skip top level (which is 'clicon' or something */
    if ((uv = cvec_new(0)) == NULL) 
	goto catch;
    while ((x = xml_child_each(xt, x, CX_ELMNT)) != NULL)
	if (xml2db_1(x, dbspec, dbname, uv, NULL, NULL) < 0)
	    goto catch;
    retval = 0;
  catch:
    if (uv)
	cvec_free(uv);
    return retval;
}

/*
 * save_db_to_xml
 * Transform a db into XML and save into a file
 */
int
save_db_to_xml(char *filename, dbspec_key *dbspec, char *dbname)
{
    FILE *f;
    cxobj *xn;

    if ((f = fopen(filename, "wb")) == NULL){
	clicon_err(OE_CFG, errno, "Creating file %s", filename);
	return -1;
    } 
    if ((xn = db2xml(dbname, dbspec, "clicon")) == NULL) {
	fclose(f);
	clicon_err(OE_CFG, errno, "Formatting config XML %s", filename);
	return -1;
    }

    clicon_xml2file(f, xn, 0, 0); /* pretty-print may add spaces in values */
    fclose(f);
    xml_free(xn);

    return 0;

}

/*
 * load_xml_to_db
 * Load a saved XML file into a db
 */
int
load_xml_to_db(char *xmlfile, dbspec_key *dbspec, char *dbname)
{
    int fd = -1;
    int retval = -1;
    cxobj *xt;
    cxobj *xn = NULL; 
    
    if ((fd = open(xmlfile, O_RDONLY)) < 0){
	clicon_err(OE_UNIX, errno, "%s: open(%s)", __FUNCTION__, xmlfile);
	return -1;
    }
    /* XXX: if non-xml file, why no error code? */
    if (clicon_xml_parse_file(fd, &xt, "</clicon>") < 0)
	goto catch;
    if (!xml_child_nr(xt)){
	clicon_err(OE_XML, errno, "%s: no children", __FUNCTION__);
	goto catch;
    }
    xn = xml_child_i(xt, 0);

    xml_prune(xt, xn, 0); /* kludge to remove top-level tag (eg clicon) */
    xml_parent_set(xn, NULL);
    xml_free(xt);
    if (xml2db(xn, dbspec, dbname) < 0)
	goto catch;
    retval = 0;

  catch:
    if (fd != -1)
	close(fd);
    if (xn)
	xml_free(xn);
    return retval;
}


/*! x is element and has eactly one child which in turn has none */
static int
tleaf(cxobj *x)
{
    cxobj *c;

    if (xml_type(x) != CX_ELMNT)
	return 0;
    if (xml_child_nr(x) != 1)
	return 0;
    c = xml_child_i(x, 0);
    return (xml_child_nr(c) == 0);
}

/*! Translate XML -> TEXT
 * @param[in]  level  print 4 spaces per level in front of each line
 * XXX: Why special treatment for name? 
 */
int 
xml2txt(FILE *f, cxobj *x, int level)
{
    cxobj *xe = NULL;
    int              children=0;
    char            *term;
    int              retval = -1;
    cxobj *xname;

    xe = NULL;     /* count children */
    while ((xe = xml_child_each(x, xe, -1)) != NULL)
	children++;
    if (!children){ 
	if (xml_type(x) == CX_BODY){
	    /* Kludge for escaping encrypted passwords */
	    if (strcmp(xml_name(xml_parent(x)), "encrypted-password")==0)
		term = chunk_sprintf(__FUNCTION__, "\"%s\"", xml_value(x));
	    else
		term = xml_value(x);
	}
	else{
	    fprintf(f, "%*s", 4*level, "");
	    term = xml_name(x);
	}
	fprintf(f, "%s;\n", term);
	retval = 0;
	goto done;
    }
    fprintf(f, "%*s", 4*level, "");
    /* XXX: Why special treatment for name? */
    if (strcmp(xml_name(x), "name") != 0)
	fprintf(f, "%s ", xml_name(x));
    if ((xname = xml_find(x, "name")) != NULL){
	if (children > 1)
	    fprintf(f, "%s ", xml_body(xname));
	if (!tleaf(x))
	    fprintf(f, "{\n");
    }
    else
	{
	if (!tleaf(x))
	    fprintf(f, "{\n");
    }
    xe = NULL;
    while ((xe = xml_child_each(x, xe, -1)) != NULL){
	if (xml_type(xe) == CX_ELMNT &&  (strcmp(xml_name(xe), "name")==0) && 
	    (children > 1)) /* skip if this is a name element (unless 0 children) */
	    continue;
	if (xml2txt(f, xe, level+1) < 0)
	    break;
    }
    if (!tleaf(x))
	fprintf(f, "%*s}\n", 4*level, "");
    retval = 0;
  done:
    return retval;
}

/*
 * xml2cli. Translate XML -> CLI commands
 * Howto: join strings and pass them down. 
 * Identify unique/index keywords for correct set syntax.
 * Args:
 *  f:        where to print cli commands
 *  x:        xml parse-tree (to translate)
 *  prepend0: Print this text in front of all commands.
 *  label:    memory chunk allocation label
 */
int 
xml2cli(FILE              *f, 
	cxobj             *x, 
	char              *prepend0, 
	enum genmodel_type gt,
	const char        *label)
{
    cxobj *xe = NULL;
    char            *term;
    char            *prepend;
    int              retval = -1;
    int              bool;

    if (!xml_child_nr(x)){
	if (xml_type(x) == CX_BODY)
	    term = xml_value(x);
	else
	    term = xml_name(x);
	if (prepend0)
	    fprintf(f, "%s ", prepend0);
	fprintf(f, "%s\n", term);
	retval = 0;
	goto done;
    }
    prepend = "";
    if (prepend0)
	if ((prepend = chunk_sprintf(label, "%s%s", prepend, prepend0)) == NULL)
	    goto done;
/* bool determines when to print a variable keyword:
   !leaf           T for all (ie parameter)
   index GT_NONE   F
   index GT_VARS   F
   index GT_ALL    T
   !index GT_NONE  F
   !index GT_VARS  T
   !index GT_ALL   T
 */
    bool = !leaf(x) || gt == GT_ALL || (gt == GT_VARS && !xml_index(x));
//    bool = (!x->xn_index || gt == GT_ALL);
    if (bool &&
	(prepend = chunk_sprintf(label, "%s%s%s", 
					prepend, 
					strlen(prepend)?" ":"",
				 xml_name(x))) == NULL)
	goto done;
    xe = NULL;
    /* First child is unique, then add that, before looping. */

    while ((xe = xml_child_each(x, xe, -1)) != NULL){
	if (xml2cli(f, xe, prepend, gt, label) < 0)
	    goto done;
	if (xml_index(xe)){ /* assume index is first, otherwise need one more while */
	    if (gt ==GT_ALL && (prepend = chunk_sprintf(label, "%s %s", 
							prepend, 
							xml_name(xe))) == NULL)

		goto done;
	    if ((prepend = chunk_sprintf(label, "%s %s", 
					 prepend, 
					 xml_value(xml_child_i(xe, 0)))) == NULL)
		goto done;
	}
    }
    retval = 0;
  done:
    return retval;
}

/*! Translate from xml tree to JSON 
 * XXX ugly code could be cleaned up
 */
int 
xml2json1(FILE *f, cxobj *x, int level, int eq)
{
    cxobj           *xe = NULL;
    cxobj           *x1;
    int              retval = -1;
    int              level1 = level+1;
    int              level2 = level+2;
    int              i;
    int              n;
    int              eq1;

    switch(xml_type(x)){
    case CX_BODY:
	fprintf(f, "\"%s\"", xml_value(x));
	break;
    case CX_ELMNT:
	if (eq == 2)
	    fprintf(f, "%*s", 2*level2, "");
	else{
	    fprintf(f, "%*s", 2*level1, "");
	    fprintf(f, "\"%s\": ", xml_name(x));
	}
	if (xml_body(x)!=NULL){
	    if (eq==1){
		fprintf(f, "[\n");
		fprintf(f, "%*s", 2*level2, "");
	    }
	}
	else {
	    fprintf(f, "{\n");
	}
	xe = NULL;     
	n = xml_child_nr(x);
	eq1 = 0;
	for (i=0; i<n; i++){
	    xe = xml_child_i(x, i);
	    if (xml_body(xe)!=NULL){
		if ((x1 = xml_child_i(x, i+1)) != NULL){
		    if (xml_body(x1) && strcmp(xml_name(xe), xml_name(x1)) == 0){
			if (!eq1)
			    eq1 = 1;
		    }
		    else
			if (eq1)
			    eq1 = 2; /* last */
		}
	    }
	    if (xml2json1(f, xe, level1, eq1) < 0)
		goto done;
	    if (xml_body(xe)!=NULL){
		if (eq1 == 2){
		    fprintf(f, "\n");
		    fprintf(f, "%*s", 2*level2, "");
		    fprintf(f, "]");
		}
		if (i+1<n)
		    fprintf(f, ",");
		fprintf(f, "\n");
	    }
	    if (eq1==2)
		eq1 = 0;
	}
	if (tleaf(x)){
	}
	else{
	    fprintf(f, "%*s}\n", 2*level1, "");
	}
	break;
    default:
	break;
    }
    //    fprintf(f, "%*s", 2*level, "");
    retval = 0;
 done:
    return retval;
}

int 
xml2json(FILE *f, cxobj *x, int level)
{
    int retval = 1;

    fprintf(f, "{\n");
    if (xml2json1(f, x, level, 0) < 0)
	goto done;
    fprintf(f, "}\n");
    retval = 0;
 done:
    return retval;
}


/*! Validate an XML tree with yang specification
 * @retval 
 */
int
xml_yang_validate(clicon_handle h, cxobj *xt, yang_spec *ys)
{
    int retval = -1;

    retval = 0;
    // done:
    return retval;
}

/*! Translate a single xml node to a cligen variable vector. Note not recursive 
 * @param[in]  xt   XML tree containing one top node
 * @param[in]  ys   Yang spec containing type specification of top-node of xt
 * @param[out] cvv  CLIgen variable vector. Should be freed by cvec_free()
 * @retval     0    Everything OK, cvv allocated and set
 * @retval    -1    Something wrong, clicon_err() called to set error. No cvv returned
 * Not recursive means that only one level of XML bodies is translated to cvec:s.
 * Example: 
    <a>
      <b>23</b>
      <c>88</c>
      <d>      
        <e>99</e>
      </d>
    </a> 
         --> b:23, c:88
 */
int
xml2cvec(cxobj *xt, yang_stmt *yt, cvec **cvv0)
{
    int               retval = -1;
    cvec             *cvv = NULL;
    cxobj            *xc;         /* xml iteration variable */
    yang_stmt        *ys;         /* yang spec */
    cg_var           *cv;
    cg_var           *ycv;
    char             *body;
    char             *reason = NULL;

    if ((cvv = cvec_new(0)) == NULL){
	clicon_err(OE_UNIX, errno, "cvec_new");
	goto err;
    }
    xc = NULL;
    /* Go through all children of the xml tree */
    while ((xc = xml_child_each(xt, xc, CX_ELMNT)) != NULL){
	if ((ys = yang_find_specnode((yang_node*)yt, xml_name(xc))) == NULL){
	    clicon_debug(1, "%s: %s not found under %s",
			 __FUNCTION__, xml_name(xc), yt->ys_argument);
	    continue;
	}
	if ((ycv = ys->ys_cv) != NULL){
	    if ((body = xml_body(xc)) != NULL){
		if ((cv = cvec_add_cv(cvv, ycv)) == NULL){
		    clicon_err(OE_PLUGIN, errno, "cvec_add");
		    goto err;
		}
		if (cv_parse1(body, cv, &reason) < 0){
		    clicon_err(OE_PLUGIN, errno, "cv_parse: %s", reason);
		    goto err;
		}
	    }
	}
    }
    if (debug){
	clicon_debug(1, "%s cvv:\n", __FUNCTION__);
	cvec_print(stderr, cvv);
    }
    *cvv0 = cvv;
    return 0;
 err:
    if (cvv)
	cvec_free(cvv);
    return retval;
}
