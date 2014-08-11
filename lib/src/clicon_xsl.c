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

 * XML XPATH and XSLT functions.
 */
/*
Implementation of a limited xslt xpath syntax. Some examples. Given the following
xml tree:
<aaa>
  <bbb><ccc>42</ccc></bbb>
  <bbb><ccc>99</ccc></bbb>
  <ddd><ccc>22</ccc></ddd>
</aaa>

With the follwoing xpath examples. There are some diffs and many limitations compared
to the xml standards:
	/	      # whole tree <aaa>...</aaa>
	/bbb          # All bbb's under aaa: <bbb>...</bbb><bbb>...</bbb>
	//bbb	      # All bbb's under aaa, 
	//b?b	      # All bbb's under aaa, 
	//b*	      # All bbb's under aaa, 
	//b*\/ccc      # All ccc's under all bbb:s, 
	//\*\/ccc       # All ccc's (both under bbb and ddd)
	//bbb@x       # x="hello"
	//bbb[@x]     # <bbb x="bye"><ccc>99</ccc></bbb>
	//bbb[@x=bye] # <bbb x="bye"><ccc>99</ccc></bbb>
	//bbb[0]      # <bbb><ccc>42</ccc></bbb>
	//bbb[ccc=99] # <bbb x="bye"><ccc>99</ccc></bbb>
        //\*\/[ccc=22]  # <ddd x="hello"><ccc>22</ccc></ddd>
	//bbb | //ddd   # ALl <bbb> and <ddd>. Note spaces around |
	etc

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

/* clicon */
#include "clicon_err.h"
#include "clicon_buf.h"
#include "clicon_xml.h"
#include "clicon_xsl.h"

/* Constants */
#define XPATH_VEC_START 128

/*
 * Types 
 */
struct searchvec{
    cxobj    **sv_v0;   /* here is result */
    int        sv_v0len;
    cxobj    **sv_v1;   /* this is tmp storage */
    int        sv_v1len;
    int        sv_max;
};
typedef struct searchvec searchvec;

/* move from v1 to v0 */
static int
sv_move(searchvec *sv)
{
    memcpy(sv->sv_v0, sv->sv_v1, sizeof(cxobj*)*sv->sv_v1len);
    sv->sv_v0len = sv->sv_v1len;
    sv->sv_v1len = 0;
    return 0;
}

/* alloc help-function: double xv vectors. spcial case for init */
static int
vec_alloc(searchvec *sv)
{
    cxobj           **xv0     = sv->sv_v0;
    cxobj           **xv1     = sv->sv_v1;
    int               xv_max0 = sv->sv_max;
    int               xv_max;
    int               init;

    init = (xv0 == NULL);
    if (init)
	xv_max = xv_max0;
    else
	xv_max = 2*xv_max0;
    if ((xv0 = realloc(xv0, sizeof(cxobj *) * xv_max)) == NULL){
	clicon_err(OE_XML, errno, "%s: realloc", __FUNCTION__);
	return -1;
    }
    if (init)
	memset(xv0, 0, xv_max);
    else
	memset(xv0+xv_max0, 0, xv_max-xv_max0);
    if ((xv1 = realloc(xv1, sizeof(cxobj *) * xv_max)) == NULL){
	clicon_err(OE_XML, errno, "%s: realloc", __FUNCTION__);
	return -1;
    }
    if (init)
	memset(xv1, 0, xv_max);
    else
	memset(xv1+xv_max0, 0, xv_max-xv_max0);
    sv->sv_v0 = xv0;
    sv->sv_v1 = xv1;
    sv->sv_max = xv_max;
    return 0;
}

/*! Find a node 'deep' in an XML tree
 *
 * The xv_* arguments are filled in  nodes found earlier.
 * args:
 *  @param[in]    xn_parent  Base XML object
 *  @param[in]    name       shell wildcard pattern to match with node name
 *  @param[in]    node_type  CX_ELMNT, CX_ATTR or CX_BODY
 *  @param[in,out] vec1        internal buffers with results
 *  @param[in,out] vec0        internal buffers with results
 *  @param[in,out] vec_len     internal buffers with length of vec0,vec1
 *  @param[in,out] vec_max     internal buffers with max of vec0,vec1
 * returns:
 *  0 on OK, -1 on error
 */
static int
recursive_find(cxobj   *xn, 
	       char       *pattern, 
	       int         node_type,
	       searchvec  *sv)
{
    cxobj *xsub; 
    int       retval = -1;

    xsub = NULL;
    while ((xsub = xml_child_each(xn, xsub, node_type)) != NULL) {
	if (fnmatch(pattern, xml_name(xsub), 0) == 0){
	    if (sv->sv_v1len >= sv->sv_max)
		if (vec_alloc(sv) < 0)
		    goto done;
	    sv->sv_v1[sv->sv_v1len] = xsub;
	    sv->sv_v1len += 1;
	    continue; /* Dont go deeper */
	}
	if (recursive_find(xsub, pattern, node_type, sv) < 0)
	    goto done;
    }
    retval = 0;
  done:
    return retval;
}

static int
xpath_expr(char *e, searchvec *sv)
{
    char      *e_a;
    char      *e_v;
    int        i;
    int        retval = -1;
    cxobj     *cxn;
    cxobj     *cxc;

    if (*e == '@'){ /* @ is a selection */
	e++;
	e_v=e;
	e_a = strsep(&e_v, "=");
	if (e_a == NULL){
	    clicon_err(OE_XML, errno, "%s: malformed expression: [@%s]", 
		       __FUNCTION__, e);
	    goto done;
	}
	for (i=0; i<sv->sv_v0len; i++){
	    cxn = sv->sv_v0[i];
	    if ((cxc = xml_find(cxn, e_a)) != NULL &&
		(xml_type(cxc) == CX_ATTR)){
		if (!e_v || strcmp(xml_value(cxc), e_v) == 0)
		    sv->sv_v1[sv->sv_v1len++] = cxn;
	    }
	}
    }
    else{ /* either <n> or <tag><op><value>, where <op>='=' for now */
	int oplen = strcspn(e, "=");
	if (strlen(e+oplen)==0){ /* no operator */
	    if (sscanf(e, "%d", &i) == 1){ /* number */
		if (i < sv->sv_v0len){
		    cxc = sv->sv_v0[i]; /* XXX: cant compress: gcc breaks */
		    sv->sv_v1[sv->sv_v1len++] = cxc;
		}
	    }
	    else{
		clicon_err(OE_XML, errno, "%s: malformed expression: [%s]", 
			   __FUNCTION__, e);
		goto done;
	    }
	}
	else{
	    char *tag, *val;
	    if ((tag = strsep(&e, "=")) == NULL){
		clicon_err(OE_XML, errno, "%s: malformed expression: [%s]", 
			   __FUNCTION__, e);
		goto done;
	    }
	    for (i=0; i<sv->sv_v0len; i++){
		cxn = sv->sv_v0[i];
		if ((cxc = xml_find(cxn, tag)) != NULL &&
		    (xml_type(cxc) == CX_ELMNT)){
		    if ((val = xml_body(cxc)) != NULL &&
			strcmp(val, e) == 0){
			sv->sv_v1[sv->sv_v1len++] = cxn;
		    }
		}
	    }
	}
    }
    /* copy the array from 1 to 0 */
    sv_move(sv);
    retval = 0;
  done:
    return retval;
}

static int
xpath_loop(char *xpathstr, char **pathexpr)
{
    int retval = -1;
    int len;
    int i;

    char *pe = NULL;
    len = strlen(xpathstr) - 1;
    if (xpathstr[len] == ']'){
	xpathstr[len] = '\0';
	len = strlen(xpathstr) - 1; /* recompute due to null */
	for (i=len; i; i--){
	    if (xpathstr[i] == '['){
		xpathstr[i] = '\0';
		pe = &xpathstr[i+1];
		break;
	    }
	}
	if (pe==NULL){
	    clicon_err(OE_XML, errno, "%s: mismatched []: %s", __FUNCTION__, xpathstr);
	    goto done;
	}
    }
    retval = 0;
  done:
    *pathexpr = pe;
    return retval;
}

static int
xpath_exec(cxobj *cxtop, char *xpath0, searchvec *sv)
{
    cxobj     *cxn;
    cxobj     *cxc;
    int        recurse = 0;
    char      *str;
    char      *pathexpr=NULL;
    char      *q;
    char      *xp;
    int        i;
    char      *strn;

    if ((xp = strdup(xpath0)) == NULL){
	clicon_err(OE_XML, errno, "%s: strdup", __FUNCTION__);
	return -1;
    }
    /* Transform eg "a/b[kalle]" -> "a/b" e="kalle" */
    if (xpath_loop(xp, &pathexpr) < 0)
	return -1;
    /* first look for expressions regarding attrs */
    str = xp;
    q = strsep(&str, "@");
    if (*q != '/') /* This is kinda broken */
	goto done;
    q++;
    /* These are search arrays allocated in sub-function used later */
    if (vec_alloc(sv) < 0)
	return -1;
    /* Initialize vec0 */
    sv->sv_v0len = 1;
    sv->sv_v0[0] = cxtop;
    sv->sv_v1len = 0;

    while (q && strlen(q) && sv->sv_v0len){
	recurse = 0;
	if (*q == '/') {
	    q++;
	    recurse = 1;
	}
	strn = strsep(&q, "/");
	if (strn== NULL || strlen(strn) == 0){
	    if (!str || !recurse){ /* eg //@ */
		/* copy the array from 1 to 0 */
		sv_move(sv);
	    }
	    break;
	}
	/* Fill in the searchv with values from next values */
	for (i=0; i<sv->sv_v0len; i++){
	    cxn = sv->sv_v0[i];
	    if (!recurse){
		cxc = NULL;
		while ((cxc = xml_child_each(cxn, cxc, CX_ELMNT)) != NULL) {
		    if (fnmatch(strn, xml_name(cxc), 0) == 0 &&
			xml_type(cxc) == CX_ELMNT){

			if (sv->sv_v1len >= sv->sv_max)
			    if (vec_alloc(sv) < 0)
				return -1;
		    }
		}
	    }
	    else{
		if (recursive_find(cxn, strn, CX_ELMNT, sv) < 0)
		    return -1;
	    }
	}
	/* copy the array from 1 to 0 */
	sv_move(sv);
    }
    if (str){
	for (i=0; i<sv->sv_v0len; i++){
	    cxn = sv->sv_v0[i];
	    if (recurse){
		if (recursive_find(cxn, str, CX_ATTR, sv) < 0)
		    return -1;
	    }
	    else{
		cxc = xml_find(cxn, str);
		if (cxc && xml_type(cxc) == CX_ATTR)
		    sv->sv_v1[sv->sv_v1len++] = cxc;
	    }
	}
	/* copy the array from 1 to 0 */
	sv_move(sv);
    } /* str */
    if (pathexpr != NULL)
	if (xpath_expr(pathexpr, sv) < 0)
	    return -1;
  done:
    if (xp)
	free(xp);
    /* result in sv->sv_v0 */
    if (sv){
	if (sv->sv_v1)
	    free(sv->sv_v1);
	sv->sv_v1len = 0;
	sv->sv_v1 = NULL;
    }
    return 0;
} /* xpath_exec */


/*
 * intermediate function to handle the 'or' case. For example: 
 * xpath = //a | //b. xpath_first+ splits xpath up in several subcalls
 * (eg xpath=//a and xpath=//b) and collects the results.
 * Note: if a match is found in both, two (or more) same results will be 
 * returned.
 * Note, this could be 'folded' into xpath1 but I judged it too complex.
 */
static cxobj **
xpath_internal(cxobj *cxtop, char *xpath0, int *vec_len00)
{
    char             *s0;
    char             *s1;
    char             *s2;
    char             *xpath;
    cxobj           **vec0 = NULL;
    int               vec_len0;
    searchvec        *sv;

    if ((sv = malloc(sizeof *sv)) == NULL){
	clicon_err(OE_XML, errno, "%s: malloc", __FUNCTION__);
	return NULL;
    }
    vec_len0 = 0;
    if ((s0 = strdup(xpath0)) == NULL){
	clicon_err(OE_XML, errno, "%s: strdup", __FUNCTION__);
	return NULL;
    }
    s2 = s1 = s0;
    memset(sv, 0, sizeof(*sv));
    sv->sv_max = XPATH_VEC_START;
    while (s1 != NULL){
	s2 = strstr(s1, " | ");
	if (s2 != NULL){
	    *s2 = '\0'; /* terminate xpath */
	    s2 += 3;
	}
	xpath = s1;
	s1 = s2;
	if (xpath_exec(cxtop, xpath, sv) < 0)
	    goto done;
	if (sv->sv_v0len > 0){
	    vec_len0 += sv->sv_v0len;
	    if ((vec0 = realloc(vec0, sizeof(cxobj *) * vec_len0)) == NULL){	
		clicon_err(OE_XML, errno, "%s: realloc", __FUNCTION__);
		goto done;
	    }
	    memcpy(vec0+vec_len0-sv->sv_v0len, 
		   sv->sv_v0, 
		   sv->sv_v0len*sizeof(cxobj *));
	}
	if (sv->sv_v0)
	    free(sv->sv_v0);
	memset(sv, 0, sizeof(*sv));
	sv->sv_max = XPATH_VEC_START;
    }
  done:
    if (sv){
	if (sv->sv_v1)
	    free(sv->sv_v1);
	if (sv->sv_v0)
	    free(sv->sv_v0);
	free(sv);
    }
    if (s0)
	free(s0);
    *vec_len00 = vec_len0;
    return vec0;
}

/*! A restricted xpath function where the first matching entry is returned
 * See xpath1() on details for subset.
 * args:
 * @param[in]  cxtop  xml-tree where to search
 * @param[in]  xpath   string with XPATH syntax
 * @retval     xml-tree of first match, or NULL on error. 
 *
 * Note that the returned pointer points into the original tree so should not be freed
 * after use.
 * See also xpath_each, xpath_vec.
 */
cxobj *
xpath_first(cxobj *cxtop, char *xpath)
{
    cxobj **vec0;
    int vec0_len = 0;
    cxobj *xn = NULL;

    if ((vec0 = xpath_internal(cxtop, xpath, &vec0_len)) == NULL)
	goto done;
    if (vec0_len)
	xn = vec0[0];
    else
	xn = NULL;
  done:
    if (vec0)
	free(vec0);
    return xn;

}

/*! A restricted xpath iterator that loops over all matching entries
 *
 * See xpath1() on details for subset.
 * @code
 *   cxobj *x = NULL;
 *   while ((x = xpath_each(cxtop, "//symbol/foo", x)) != NULL) {
 *     ...
 *   }
 * @endcode
 * @param[in]  cxtop  xml-tree where to search
 * @param[in]  xpath   string with XPATH syntax
 * @param[in]  xprev   iterator/result should be initiated to NULL
 * @retval     xml-tree of n:th match, or NULL on error. 
 *
 * Note that the returned pointer points into the original tree so should not be freed
 * after use.
 * See also xpath, xpath_vec.
 * NOTE: uses a static variable: consider replacing with xpath_vec() instead
 */
cxobj *
xpath_each(cxobj *cxtop, char *xpath, cxobj *xprev)
{
    static    cxobj **vec0 = NULL; /* XXX */
    static int        vec0_len = 0;
    cxobj            *xn = NULL;
    int i;
    
    if (xprev == NULL){
	if (vec0) // XXX
	    free(vec0); // XXX
	vec0_len = 0;
	if ((vec0 = xpath_internal(cxtop, xpath, &vec0_len)) == NULL)
	    goto done;
    }
    if (vec0_len){
	if (xprev==NULL)
	    xn = vec0[0];
	else{
	    for (i=0; i<vec0_len; i++)
		if (vec0[i] == xprev)
		    break;
	    if (i>=vec0_len-1)
		xn = NULL; 
	    else
		xn = vec0[i+1];
	}
    }
    else
	xn = NULL;
  done:
    return xn;
}

/*! A restricted xpath that returns a vector of macthes
 *
 * See xpath1() on details for subset.
 * @code
 *   cxobj **xv;
 *   int               xlen;
 *   if ((xv = xpath_vec(cxtop, "//symbol/foo", &xlen)) != NULL) {
 *      for (i=0; i<xlen; i++){
 *         xn = xv[i];
 *         ...
 *      }
 *      free(xv);
 *   }
 * @endcode
 * @param[in]  cxtop  xml-tree where to search
 * @param[in]  xpath   string with XPATH syntax
 * @param[out] xv_len  returns length of vector in return value
 * @retval   vector of xml-trees, or NULL on error. Vector must be freed after use
 *
 * Note that although the returned vector must be freed after use, the returned xml
 * trees need not be.
 * See also xpath, xpath_each.
 */
cxobj **
xpath_vec(cxobj *cxtop, char *xpath, int *xv_len)
{
    return xpath_internal(cxtop, xpath, xv_len);
}
