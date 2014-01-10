/*
 *  CVS Version: $Id: xmlgen_xsl.c,v 1.10 2013/08/01 09:15:47 olof Exp $
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
#include "xmlgen_xf.h"
#include "xmlgen_xml.h"
#include "xmlgen_xsl.h"

/* alloc help-function: double xv vectors. spcial case for init */
static int
xv_alloc(struct xml_node ***xv0p, struct xml_node ***xv1p, int *xv_maxp)
{
    struct xml_node **xv0     = *xv0p;
    struct xml_node **xv1     = *xv1p;
    int               xv_max0 = *xv_maxp;
    int               xv_max;
    int               init;

    init = (xv0p == NULL);
    if (init)
	xv_max = xv_max0;
    else
	xv_max = 2*xv_max0;
    if ((xv0 = realloc(xv0, sizeof(struct xml_node *) * xv_max)) == NULL){
	clicon_err(OE_XML, errno, "%s: realloc", __FUNCTION__);
	return -1;
    }
    if (init)
	memset(xv0, 0, xv_max);
    else
	memset(xv0+xv_max0, 0, xv_max-xv_max0);
    if ((xv1 = realloc(xv1, sizeof(struct xml_node *) * xv_max)) == NULL){
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

/*
 * Similar to xml_find but find a node with name "name" recursively
 * in the tree. 
 * Return NULL if no such node found.
 * Send in a vector which is filled in with matching nodes.
 * XXX: Only match ELEMENT objects.
 */
static int
xml_find_deep(struct xml_node *xn_parent, char *name, 
	      struct xml_node ***xv1, 
	      struct xml_node ***xv0, 
	      size_t *xv_len, 
	      int *xv_max,
	      int node_type)
{
    int i;
    struct xml_node *xn = NULL; 

    for (i=0; i<xn_parent->xn_nrchildren; i++){
	xn = xn_parent->xn_children[i];
	if (xn->xn_type == node_type && strcmp(name, xn->xn_name) == 0){
	    if (*xv_len >= *xv_max)
		if (xv_alloc(xv0, xv1, xv_max) < 0)
		    return -1;
	    (*xv1)[(*xv_len)] = xn;
	    *xv_len = *xv_len + 1;
	    continue; /* Dont go deeper */
	}
	if (xml_find_deep(xn, name, xv1, xv0, xv_len, xv_max, node_type) < 0)
	    return -1;
    }
    return 0;
}


/*
 * xpath1
 * sub-function to xpath(), xpath_each(), xpath_vec() that actually does the xpathing
 * XXX: or with ///?
 * Given a node, and a very limited XPATH syntax, return a vector of xml node 
 * that matches.
 * examples: 
        /foobar
        //foobar

        /foobar@bar
        //foobar@bar
        /@bar
        //@bar
	/foo/bar 
	/foo[bar=89]  # select //foo where body of sub-element bar equals '89'
	//foo/bar/fie@attr, osv
	//foo[n] Select nth foo element
	//foo[@bar] Select all foo elemtn with bar attribute
	//foo[@bar=5] Select all foo elements with bar attribute equal to 5

 */

static struct xml_node **
xml_xpath1(struct xml_node *xn_top, char *xpath0, int *xv00_len)
{
    int deep = 0;
    char *attr;
    char *expr=NULL;
#if 1
    char *e_attr=NULL, *e_val=NULL;
#endif
    char *p;
    char *node;
    char *xpath = strdup(xpath0);
    struct xml_node *xn, *xc;
    struct xml_node **xv0=NULL;  /* Search vector 0 */
    struct xml_node **xv1=NULL;  /* Search vector 1 */
    int i,j;
    int xv_max = 128;
    size_t xv0_len=0, xv1_len=0;

    assert(xn_top);
    if (xdebug)
	fprintf(stderr, "xpath: %s\n", xpath);
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
    if (xdebug > 1)
	fprintf(stderr, "pre: %s %d\n", p, (int)strlen(p)); 
    while (p && strlen(p) && xv0_len){
	if (xdebug > 1){
	    fprintf(stderr, "node:\n"); 
	    for (i=0; i<xv0_len; i++){
//		fprintf(stderr, "  [%d:]%s\n", i, xv0[i]->xn_name); 
//		xml_to_file(stderr, xv0[i], 4, 1);
//		fprintf(stderr, "\n"); 
	    }
	}
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
		memcpy(xv0, xv1, sizeof(struct xml_node*)*xv1_len);
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
		if (strcmp(node, xn->xn_name) == 0){
		    xv1[(xv1_len)] = xn;
		    xv1_len++;
		}
		else
#endif
		    if (xml_find_deep(xn, node, &xv1, &xv0, &xv1_len, &xv_max, XML_ELEMENT) < 0)
		    return NULL;
	    }
	    else
		for (j=0; j<xn->xn_nrchildren; j++){
		    xc = xn->xn_children[j];
		    if (strcmp(node, xc->xn_name) == 0 &&
			xc->xn_type == XML_ELEMENT){

			if (xv1_len >= xv_max)
			    if (xv_alloc(&xv0, &xv1, &xv_max) < 0)
				return NULL;
			xv1[xv1_len++] = xc;
		    }
		}
	}
	/* Now move search vector from xv1->xv0 */
	memcpy(xv0, xv1, sizeof(struct xml_node*)*xv1_len);
	xv0_len = xv1_len;
	xv1_len = 0;
    }
    if (xdebug > 1){
	fprintf(stderr, "element:\n"); 
	for (i=0; i<xv0_len; i++){
	    fprintf(stderr, "  [%d:]%s\n", i, xv0[i]->xn_name); 
	    xml_to_file(stderr, xv0[i], 4, 1);
	    fprintf(stderr, "\n"); 
	}
    } 
    if (attr){
	for (i=0; i<xv0_len; i++){
	    xn = xv0[i];
	    if (deep){
		if (xml_find_deep(xn, attr, &xv1, &xv0, &xv1_len, &xv_max, XML_ATTRIBUTE) < 0)
		    return NULL;
	    }
	    else{
		xc = xml_find(xn, attr);
		if (xc && xc->xn_type == XML_ATTRIBUTE)
		    xv1[xv1_len++] = xc;
	    }
	}
	/* Now move search vector from xv1->xv0 */
	memcpy(xv0, xv1, sizeof(struct xml_node*)*xv1_len);
	xv0_len = xv1_len;
	xv1_len = 0;
	if (xdebug > 1){
	    fprintf(stderr, "attr:\n"); 
	    for (i=0; i<xv0_len; i++){
		fprintf(stderr, "  [%d:]%s\n", i, xv0[i]->xn_name); 
		xml_to_file(stderr, xv0[i], 4, 1);
		fprintf(stderr, "\n"); 
	    }
	}
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
		    (xc->xn_type == XML_ATTRIBUTE)){
		    if (!e_val || strcmp(xc->xn_value, e_val) == 0)
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
			(xc->xn_type == XML_ELEMENT)){
			if ((val = xml_get_body(xc)) != NULL &&
			    strcmp(val, expr) == 0){
			    xv1[xv1_len++] = xn;
			}
		    }
		}
	    }
	}
	/* Now move search vector from xv1->xv0 */
	memcpy(xv0, xv1, sizeof(struct xml_node*)*xv1_len);
	xv0_len = xv1_len;
	xv1_len = 0;
	if (xdebug > 1){
	    fprintf(stderr, "expr:\n"); 
	    for (i=0; i<xv0_len; i++){
		fprintf(stderr, "  [%d:]%s\n", i, xv0[i]->xn_name); 
		xml_to_file(stderr, xv0[i], 4, 1);
		fprintf(stderr, "\n"); 
	    }
	}
    } /* expr */
  done:
    /* Result in xv0 (if any). Pick result after previous or first of NULL */
    if (0){
	fprintf(stderr, "%s: result:\n", __FUNCTION__); 
	for (i=0; i<xv0_len; i++)
	    fprintf(stderr, "\t%s\n", xv0[i]->xn_name); 
    }
    if (xpath)
	free(xpath);
    *xv00_len = xv0_len;
    if (xv1)
	free(xv1);
    return xv0;
} /* xml_xpath */


/*
 * xpath
 * Given a node, and a very limited XPATH syntax, return the first entry that matches.
 * See xpath1() on details for subset.
 * args:
 *  IN  xn_top  xml-tree where to search
 *  IN  xpath   string with XPATH syntax
 * returns:
 *   xml-tree of first match, or NULL on error. 
 * Note that the returned pointer points into the original tree so should not be freed
 * after use.
 * See also xpath_each, xpath_vec.
 */
struct xml_node *
xml_xpath(struct xml_node *xn_top, char *xpath)
{
    struct xml_node **xv0;
    int xv0_len = 0;
    struct xml_node *xn = NULL;

    if ((xv0 = xml_xpath1(xn_top, xpath, &xv0_len)) == NULL)
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

/*
 * xpath_each
 * xpath iterator
 * Given a node, and a very limited XPATH syntax, iteratively return the matches.
 * See xpath1() on details for subset.
 * Example of usage:
 *   struct xml_node *x = NULL;
 *   while ((x = xpath_each(xn_top, "//symbol/foo", x)) != NULL) {
 *     ...
 *   }
 * args:
 *  IN  xn_top  xml-tree where to search
 *  IN  xpath   string with XPATH syntax
 *  IN  xprev   iterator/result should be initiated to NULL
 * returns:
 *   xml-tree of n:th match, or NULL on error. 
 * Note that the returned pointer points into the original tree so should not be freed
 * after use.
 * See also xpath, xpath_vec.
 * NOTE: uses a static variable: consider replacing with xpath_vec() instead
 */
struct xml_node *
xpath_each(struct xml_node *xn_top, char *xpath, struct xml_node *xprev)
{
    static    struct xml_node **xv0 = NULL; /* XXX */
    static int xv0_len = 0;
    struct xml_node *xn = NULL;
    int i;
    
    if (xprev == NULL){
	if (xv0) // XXX
	    free(xv0); // XXX
	xv0_len = 0;
	if ((xv0 = xml_xpath1(xn_top, xpath, &xv0_len)) == NULL)
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

/*
 * xpath_vec
 * xpath iterator
 * Given a node, and a very limited XPATH syntax, return a vector of matches.
 * See xpath1() on details for subset.
 * Example of usage:
 *   struct xml_node **xv;
 *   int               xlen;
 *   if ((xv = xpath_vec(xn_top, "//symbol/foo", &xlen)) != NULL) {
 *      for (i=0; i<xlen; i++){
 *         xn = xv[i];
 *         ...
 *      }
 *      free(xv);
 *   }
 * args:
 *  IN  xn_top  xml-tree where to search
 *  IN  xpath   string with XPATH syntax
 *  OUT xv_len  returns length of vector in return value
 * returns:
 *   vector of xml-trees, or NULL on error. Vector must be freed after use
 * Note that although the returned vector must be freed after use, the returned xml
 * trees need not be.
 * See also xpath, xpath_each.
 */
struct xml_node **
xpath_vec(struct xml_node *xn_top, char *xpath, int *xv_len)
{
    struct xml_node **xv;

    xv = xml_xpath1(xn_top, xpath, xv_len);
    return xv;
}


/*
 *---------------------------------------
 * here starts xslt code
 * XSLT Implementation of 
 *    xsl:template 
 *    xsl:value-of 
 *    xsl:for-each
 *    xsl:if
 *    xsl:choose
 *
 * Naming convention:
 *    Everything starting with 's' is xlst stylesheet
 *    Everything starting with 'x' is xml
 *    Everything starting with 'r' is result xmlxs
 */
/* forward declaration */
static int
traverse(struct xml_node *st, struct xml_node *xt, struct xml_node *rtop);

/*
 * <xsl:value-of select="expr"/>
 */
static int
value_of(struct xml_node *st, struct xml_node *xt, struct xml_node *rt)
{
    char *select;
    struct xml_node *x;
    struct xml_node *rb;
    char *v;

    if ((select = xml_get(st, "select")) == NULL)
	return 0;
    if ((x = xml_xpath(xt, select)) == NULL)
	return 0;
    if ((v = xml_get_body(x)) == NULL)
	return 0;
    if ((rb = xml_new("body", rt)) == NULL)
	return 0;
    rb->xn_type = XML_BODY;
    rb->xn_value = strdup(v);
    return 0;
}

/*
 * <xsl:for-each select="expr"/>
 */
static int
for_each(struct xml_node *st, struct xml_node *xt, struct xml_node *rt)
{
    char *select;
    struct xml_node *x = NULL;
    struct xml_node *s;
    int i;

    if ((select = xml_get(st, "select")) == NULL)
	return 0;
    while ((x = xpath_each(xt, select, x))) {
       for (i=0; i<st->xn_nrchildren; i++){
	   s = st->xn_children[i];
	   if (s->xn_type == XML_ELEMENT)
	       if (traverse(s, x, rt) < 0)
		   return -1;
       }
   }
    return 0;
}

/*
 * <xsl:if test="expr"/>
 */
static int
ifelement(struct xml_node *st, struct xml_node *xt, struct xml_node *rt)
{
    char *test;
    struct xml_node *s = NULL;

    if ((test = xml_get(st, "test")) == NULL)
	return 0;
    if (xml_xpath(xt, test) == NULL)
	return 0;
    while ((s = xml_child_each(st, s, XML_ELEMENT)) != NULL) 
	if (traverse(s, xt, rt) < 0)
	    return -1;
    return 0;
}

/*
 * <xsl:choose>
 *   <xsl:when test="expr"/>
 *   <xsl:when test="expr"/>
 *   <xsl:otherwise/>
 * </xsl:choose>
 */
static int
choose_test(struct xml_node *st, struct xml_node *xt, struct xml_node *rt, int *match)
{
    char *test;
    struct xml_node *s = NULL;

    *match = 0;
    if ((test = xml_get(st, "test")) == NULL)
	return 0;
    if (xml_xpath(xt, test) == NULL)
	return 0;
    *match = 1;
    while ((s = xml_child_each(st, s, -1)) != NULL) {
	if (s->xn_type == XML_ATTRIBUTE)
	    continue;
	if (traverse(s, xt, rt) < 0)
	    return -1;
    }
    return 0;
}

static int
choose(struct xml_node *st, struct xml_node *xt, struct xml_node *rt)
{
    struct xml_node *s = NULL;
    struct xml_node *sc = NULL;
    int match = 0;

    /* first try matching 'when' statement */
    while ((s = xml_child_each(st, s, XML_ELEMENT)) != NULL) {
	if (s->xn_namespace && strcmp(s->xn_namespace, "xsl") == 0){ 
	    if (strcmp(s->xn_name, "when")==0){
		if (choose_test(s, xt, rt, &match) < 0)
		    return -1;
		if (match)
		    break;
	    }
	}
    }
    if (!match){ /* no match above - find otherwise */
	if ((s = xml_xpath(st, "/otherwise")) != NULL &&
	    s->xn_namespace && 
	    strcmp(s->xn_namespace, "xsl") == 0){
	    while ((sc = xml_child_each(s, sc, XML_ELEMENT)) != NULL) 
		if (traverse(sc, xt, rt) < 0)
		    return -1;
	}
    }
    return 0;
}


/*
 * stm is top of template. Create a new parse-tree and copy to it.
 */
static int
traverse(struct xml_node *st, struct xml_node *xt, struct xml_node *rtop)
{
    struct xml_node *rt;
    int i;

    if (st->xn_namespace && strcmp(st->xn_namespace, "xsl") == 0){ 
	/* do intelligent stuff */
	if (strcmp(st->xn_name, "value-of")==0)
	    value_of(st, xt, rtop);
	else
	if (strcmp(st->xn_name, "for-each")==0)
	    for_each(st, xt, rtop);
	else
	if (strcmp(st->xn_name, "if")==0)
	    ifelement(st, xt, rtop);
	else
	if (strcmp(st->xn_name, "choose")==0)
	    choose(st, xt, rtop);
    }
    else{ /* just copy */
	if ((rt = xml_new(st->xn_name, rtop)) == NULL)
	    return -1;
	xml_cp1(st, rt);
	for (i=0; i<st->xn_nrchildren; i++)
	    if (traverse(st->xn_children[i], xt, rt) < 0)
		return -1;
    }
    return 0;
}

/*
 *  <xsl:template match="expr"/>
 */
static struct xml_node *
template(struct xml_node *stop, struct xml_node *xtop)
{
    struct xml_node *st;
    struct xml_node *s;
    struct xml_node *xt;
    struct xml_node *rtop;
    char *match;
    int i;
    
    if ((st = xml_xpath(stop, "//template")) == NULL)
	return 0;
    if (st->xn_namespace==NULL || strcmp(st->xn_namespace, "xsl"))
	return 0;
    if (xdebug)
	fprintf(stderr, "%s: template found\n", __FUNCTION__);
    if ((match = xml_get(st, "match")) == NULL)
	return 0;
    if (xdebug)
	fprintf(stderr, "%s: match:%s\n", __FUNCTION__, match);
    if (xdebug)
	fprintf(stderr, "%s: xtop name:%s\n", __FUNCTION__, xtop->xn_name);
    if ((xt = xml_xpath(xtop, match)) == NULL)
	return 0;
    if (xdebug)
	fprintf(stderr, "%s: match obj:%s\n", __FUNCTION__, xt->xn_name);
    if ((rtop = xml_new("top", NULL)) == NULL)
	return NULL;
    for (i=0; i<st->xn_nrchildren; i++){
	s = st->xn_children[i];
	if (s->xn_type == XML_ELEMENT){
	    if (traverse(s, xt, rtop) < 0)
		break;
	}
    }
    return (i==st->xn_nrchildren)?rtop:NULL;
}


/*
 * xml_xslt 
 * Limited xslt translation
 */
struct xml_node *
xml_xslt(struct xml_node *st, struct xml_node *xt)
{
    return template(st, xt);
}

