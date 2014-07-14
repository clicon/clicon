/*
 *  CVS Version: $Id: xmlgen_xsl.c,v 1.10 2013/08/01 09:15:47 olof Exp $
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

/* alloc help-function: double xv vectors. spcial case for init */
static int
xv_alloc(cxobj ***xv0p, cxobj ***xv1p, int *xv_maxp)
{
    cxobj **xv0     = *xv0p;
    cxobj **xv1     = *xv1p;
    int               xv_max0 = *xv_maxp;
    int               xv_max;
    int               init;

    init = (xv0p == NULL);
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
    *xv0p = xv0;
    *xv1p = xv1;
    *xv_maxp = xv_max;
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
	       cxobj ***vec1, 
	       cxobj ***vec0, 
	       int        *vec_len, 
	       int        *vec_max)
{
    cxobj *xsub; 
    int       retval = -1;

    xsub = NULL;
    while ((xsub = xml_child_each(xn, xsub, node_type)) != NULL) {
	if (fnmatch(pattern, xml_name(xsub), 0) == 0){
	    if (*vec_len >= *vec_max)
		if (xv_alloc(vec0, vec1, vec_max) < 0)
		    goto done;
	    (*vec1)[(*vec_len)] = xsub; 
	    *vec_len = *vec_len + 1;
	    continue; /* Dont go deeper */
	}
	if (recursive_find(xsub, pattern, node_type, vec1, vec0, vec_len, vec_max) < 0)
	    goto done;
    }
    retval = 0;
  done:
    return retval;
}

static cxobj **
xpath_exec(cxobj *xn_top, char *xpath0, int *xv00_len)
{
    int        deep = 0;
    char      *attr;
    char      *expr=NULL;
    char      *e_attr=NULL;
    char      *e_val=NULL;
    char      *p;
    char      *node;
    char      *xpath = strdup(xpath0);
    cxobj  *xn, *xc;
    cxobj **xv0=NULL;  /* Search vector 0 */
    cxobj **xv1=NULL;  /* Search vector 1 */
    int        i;
    int        xv_max = 128;
    int        xv0_len=0;
    int        xv1_len=0;

    assert(xn_top);

    /* Transform eg "a/b[kalle]" -> "a/b" expr="kalle" */
    if (xpath[strlen(xpath)-1] == ']'){
	xpath[strlen(xpath)-1] = '\0';
	for (i=strlen(xpath)-1; i; i--){
	    if (xpath[i] == '['){
		xpath[i] = '\0';
		expr = &xpath[i+1];
		break;
	    }
	}
	if (expr==NULL){
	    clicon_err(OE_XML, errno, "%s: mismatched []: %s", __FUNCTION__, xpath0);
	    return NULL;
	}
    }
    /* Start with attributes */
    attr = xpath;
    p = strsep(&attr, "@");
    if (*p != '/') /* Only accept root node expressions */
	goto done;
    p++;
    /* Allocate search vector */
    if (xv_alloc(&xv0, &xv1, &xv_max) < 0)
	return NULL;
    /* Initialize xv0 */
    xv0_len=0;
    xv0[xv0_len++] = xn_top;
    xv1_len = 0;

    while (p && strlen(p) && xv0_len){
	deep = 0;
	if (*p == '/') {
	    p++;
	    deep = 1;
	}
	node = strsep(&p, "/");
	if (node== NULL || strlen(node) == 0){
	    if (attr && deep) /* eg //@ */
		;
	    else{
		/* Now move search vector from xv1->xv0 */
		memcpy(xv0, xv1, sizeof(cxobj*)*xv1_len);
		xv0_len = xv1_len;
		xv1_len = 0;
	    }
	    break;
	}
	/* Fill in search vector xv1 with next level values */
	for (i=0; i<xv0_len; i++){
	    xn = xv0[i];
	    if (deep){
/* We should really also match top-level symbol */
#ifdef dontdare
		if (strcmp(node, xml_name(xn)) == 0){
		    xv1[(xv1_len)] = xn;
		    xv1_len++;
		}
		else
#endif
		    if (recursive_find(xn, node, CX_ELMNT, &xv1, &xv0, &xv1_len, &xv_max) < 0)
		    return NULL;
	    }
	    else{
		xc = NULL;
		while ((xc = xml_child_each(xn, xc, CX_ELMNT)) != NULL) {
		    if (fnmatch(node, xml_name(xc), 0) == 0 &&
			xml_type(xc) == CX_ELMNT){

			if (xv1_len >= xv_max)
			    if (xv_alloc(&xv0, &xv1, &xv_max) < 0)
				return NULL;
			xv1[xv1_len++] = xc;
		    }
		}
	    }
	}
	/* Now move search vector from xv1->xv0 */
	memcpy(xv0, xv1, sizeof(cxobj*)*xv1_len);
	xv0_len = xv1_len;
	xv1_len = 0;
    }
    if (attr){
	for (i=0; i<xv0_len; i++){
	    xn = xv0[i];
	    if (deep){
		if (recursive_find(xn, attr, CX_ATTR, &xv1, &xv0, &xv1_len, &xv_max) < 0)
		    return NULL;
	    }
	    else{
		xc = xml_find(xn, attr);
		if (xc && xml_type(xc) == CX_ATTR)
		    xv1[xv1_len++] = xc;
	    }
	}
	/* Now move search vector from xv1->xv0 */
	memcpy(xv0, xv1, sizeof(cxobj*)*xv1_len);
	xv0_len = xv1_len;
	xv1_len = 0;
    } /* attr */
    if (expr){
	if (*expr == '@'){ /* select attribute */
	    expr++;
	    e_val=expr;
	    e_attr = strsep(&e_val, "=");
	    if (e_attr == NULL){
		clicon_err(OE_XML, errno, "%s: malformed expression: [@%s]", 
			__FUNCTION__, expr);
		return NULL;
	    }
	    for (i=0; i<xv0_len; i++){
		xn = xv0[i];
		if ((xc = xml_find(xn, e_attr)) != NULL &&
		    (xml_type(xc) == CX_ATTR)){
		    if (!e_val || strcmp(xml_value(xc), e_val) == 0)
			xv1[xv1_len++] = xn;
		}
	    }
	}
	else{ /* either <n> or <tag><op><value>, where <op>='=' for now */
	    int oplen = strcspn(expr, "=");
	    if (strlen(expr+oplen)==0){ /* no operator */
		if (sscanf(expr, "%d", &i) == 1){ /* number */
		    if (i < xv0_len){
			xc = xv0[i]; /* XXX: cant compress: gcc breaks */
			xv1[xv1_len++] = xc;
		    }
		}
		else{
		    clicon_err(OE_XML, errno, "%s: malformed expression: [%s]", 
			    __FUNCTION__, expr);
		    return NULL;
		}
	    }
	    else{
		char *tag, *val;
		if ((tag = strsep(&expr, "=")) == NULL){
		    clicon_err(OE_XML, errno, "%s: malformed expression: [%s]", 
			    __FUNCTION__, expr);
		    return NULL;
		}
		for (i=0; i<xv0_len; i++){
		    xn = xv0[i];
		    if ((xc = xml_find(xn, tag)) != NULL &&
			(xml_type(xc) == CX_ELMNT)){
			if ((val = xml_body(xc)) != NULL &&
			    strcmp(val, expr) == 0){
			    xv1[xv1_len++] = xn;
			}
		    }
		}
	    }
	}
	/* Now move search vector from xv1->xv0 */
	memcpy(xv0, xv1, sizeof(cxobj*)*xv1_len);
	xv0_len = xv1_len;
	xv1_len = 0;
    } /* expr */
  done:
    /* Result in xv0 (if any). Pick result after previous or first of NULL */
    if (0){
	fprintf(stderr, "%s: result:\n", __FUNCTION__); 
	for (i=0; i<xv0_len; i++)
	    fprintf(stderr, "\t%s\n", xml_name(xv0[i])); 
    }
    if (xpath)
	free(xpath);
    *xv00_len = xv0_len;
    if (xv1)
	free(xv1);
    return xv0;
} /* xpath_first */


/*
 * intermediate function to handle the 'or' case. For example: 
 * xpath = //a | //b. xpath_first+ splits xpath up in several subcalls
 * (eg xpath=//a and xpath=//b) and collects the results.
 * Note: if a match is found in both, two (or more) same results will be 
 * returned.
 * Note, this could be 'folded' into xpath1 but I judged it too complex.
 */
static cxobj **
xpath_internal(cxobj *xn_top, char *xpath0, int *xv_len00)
{
    char             *s0;
    char             *s1;
    char             *s2;
    char             *xpath;
    cxobj **xv0 = NULL;
    cxobj **xv;
    int               xv_len0;
    int               xv_len;

    xv_len0 = 0;
    if ((s0 = strdup(xpath0)) == NULL){
	clicon_err(OE_XML, errno, "%s: strdup", __FUNCTION__);
	return NULL;
    }
    s2 = s1 = s0;
    while (s1 != NULL){
	s2 = strstr(s1, " | ");
	if (s2 != NULL){
	    *s2 = '\0'; /* terminate xpath */
	    s2 += 3;
	}
	xpath = s1;
	s1 = s2;
	xv_len = 0;
	if ((xv = xpath_exec(xn_top, xpath, &xv_len)) == NULL)
	    goto done;
	if (xv_len > 0){
	    xv_len0 += xv_len;
	    if ((xv0 = realloc(xv0, sizeof(cxobj *) * xv_len0)) == NULL){	
		clicon_err(OE_XML, errno, "%s: realloc", __FUNCTION__);
		goto done;
	    }
	    memcpy(xv0+xv_len0-xv_len, xv, xv_len*sizeof(cxobj *));

	}
	free(xv); /* may be set w/o xv_len being > 0 */
	xv = NULL;
    }
  done:
    if (xv)
	free(xv);
    if (s0)
	free(s0);
    *xv_len00 = xv_len0;
    return xv0;
}

/*! A restricted xpath function where the first matching entry is returned
 * See xpath1() on details for subset.
 * args:
 * @param[in]  xn_top  xml-tree where to search
 * @param[in]  xpath   string with XPATH syntax
 * @retval     xml-tree of first match, or NULL on error. 
 *
 * Note that the returned pointer points into the original tree so should not be freed
 * after use.
 * See also xpath_each, xpath_vec.
 */
cxobj *
xpath_first(cxobj *xn_top, char *xpath)
{
    cxobj **xv0;
    int xv0_len = 0;
    cxobj *xn = NULL;

    if ((xv0 = xpath_internal(xn_top, xpath, &xv0_len)) == NULL)
	goto done;
    if (xv0_len)
	xn = xv0[0];
    else
	xn = NULL;
  done:
    if (xv0)
	free(xv0);
    return xn;

}

/*! A restricted xpath iterator that loops over all matching entries
 *
 * See xpath1() on details for subset.
 * @code
 *   cxobj *x = NULL;
 *   while ((x = xpath_each(xn_top, "//symbol/foo", x)) != NULL) {
 *     ...
 *   }
 * @endcode
 * @param[in]  xn_top  xml-tree where to search
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
xpath_each(cxobj *xn_top, char *xpath, cxobj *xprev)
{
    static    cxobj **xv0 = NULL; /* XXX */
    static int xv0_len = 0;
    cxobj *xn = NULL;
    int i;
    
    if (xprev == NULL){
	if (xv0) // XXX
	    free(xv0); // XXX
	xv0_len = 0;
	if ((xv0 = xpath_internal(xn_top, xpath, &xv0_len)) == NULL)
	    goto done;
    }
    if (xv0_len){
	if (xprev==NULL)
	    xn = xv0[0];
	else{
	    for (i=0; i<xv0_len; i++)
		if (xv0[i] == xprev)
		    break;
	    if (i>=xv0_len-1)
		xn = NULL; 
	    else
		xn = xv0[i+1];
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
 *   if ((xv = xpath_vec(xn_top, "//symbol/foo", &xlen)) != NULL) {
 *      for (i=0; i<xlen; i++){
 *         xn = xv[i];
 *         ...
 *      }
 *      free(xv);
 *   }
 * @endcode
 * @param[in]  xn_top  xml-tree where to search
 * @param[in]  xpath   string with XPATH syntax
 * @param[out] xv_len  returns length of vector in return value
 * @retval   vector of xml-trees, or NULL on error. Vector must be freed after use
 *
 * Note that although the returned vector must be freed after use, the returned xml
 * trees need not be.
 * See also xpath, xpath_each.
 */
cxobj **
xpath_vec(cxobj *xn_top, char *xpath, int *xv_len)
{
    return xpath_internal(xn_top, xpath, xv_len);
}
