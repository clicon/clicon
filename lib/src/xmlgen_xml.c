/*
 *  CVS Version: $Id: xmlgen_xml.c,v 1.19 2013/08/17 10:54:22 benny Exp $
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

 * XML support functions.
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
#include "clicon_queue.h"
#include "clicon_chunk.h"
#include "xmlgen_xf.h"
#include "xmlgen_xml.h"
#include "xmlgen_parse.h"

/*
 * Kludge to make certain situations (eg Configure) continue
 * even though -1 is returned.
 * Note this works only on one level of recursion.
 */
int ptxml_continue_on_error = 0;

/*
 * XML debug variable. can be set in several steps
 * with increased debug detail.
 */
int xdebug = 0;

/*
 * statestr
 * simple finite state machine to detect a substring
 */
static inline int
statestr(char *tag, char ch, int state)
{
    if (tag[state] == ch)
	return state+1;
    else
	return 0;
}

/*
 * Set debug on xml library
 * return old value.
 * debug output will appear on stderr.
 */
int
xml_debug(int value)
{
    xdebug = value;
    return 0;
}

static int
xml_decode_esc(char *val, char *aval, int *i0, int *j0)
{
    int s, k;
    int i, j;

    i = *i0;
    j = *j0;
    k = 3;
    s = i - k;
    if (s >= 0){
	if (strncmp(&val[s],"&lt", k) == 0){
	    j -= k;
	    aval[j++] = '<';
	}
	if (strncmp(&val[s],"&gt", k) == 0){
	    j -= k;
	    aval[j++] = '>';
	}
    }
    k = 4;
    s = i - k;
    if (s >= 0){
	if (strncmp(&val[s],"&amp", k) == 0){
	    j -= k;
	    aval[j++] = '&';
	}
    }
    k = 5;
    s = i - k;
    if (s >= 0){
	if (strncmp(&val[s],"&apos", k) == 0){
	    j -= k;
	    aval[j++] = '\'';
	}
	if (strncmp(&val[s],"&quot", k) == 0){
	    j -= k;
	    aval[j++] = '"';
	}
    }
    *i0 = i;
    *j0 = j;
    return 0;
}

/*
 * xml_decode_attr_value
 * Translate entity references and return new allocated string.
 * &lt;     -->     <
 * &gt;     -->     >
 * &amp;    -->     &
 * &apos;   -->     '
 * &quot;   -->     "
 * XXX: but this called from xml.l in wrong place, it should be after whole 
 * string read.
 */
char*
xml_decode_attr_value(char *val)
{
  char *aval;
  int i, j;

//  assert(val);
  if ((aval = malloc(strlen(val)+1)) == NULL){
      clicon_err(OE_XML, errno, "%s: malloc", __FUNCTION__);
      return NULL;
  }
  j = 0;
  for (i=0; i<strlen(val)+1; i++){
    if (val[i] == ';')
	xml_decode_esc(val, aval, &i, &j);
    else
      aval[j++] = val[i];
  }
  return aval;
}

struct xml_node *
xml_new(char *name, struct xml_node *xn_parent)
{
    struct xml_node *xn;

    if ((xn=malloc(sizeof(struct xml_node))) == NULL){
	clicon_err(OE_XML, errno, "%s: malloc", __FUNCTION__);
	return NULL;
    }
    memset(xn, 0, sizeof(struct xml_node));
    strncpy(xn->xn_name, name, XML_FLEN-1);

    xn->xn_parent = xn_parent;
    if (xn_parent){
	xn_parent->xn_nrchildren++;
	xn_parent->xn_children = realloc(xn_parent->xn_children, 
				      xn_parent->xn_nrchildren*sizeof(struct xml_node*));
	if (xn_parent->xn_children == NULL){
	    clicon_err(OE_XML, errno, "%s: realloc", __FUNCTION__);
	    return NULL;
	}
	xn_parent->xn_children[xn_parent->xn_nrchildren-1] = xn;
    }
    return xn;
}

/*
 * xml_new_attribute
 * Like xml_new, just set it as an attribute
 */
struct xml_node *
xml_new_attribute(char *name, struct xml_node *xn_parent)
{
    xml_node_t *xn;

    if ((xn = xml_new(name, xn_parent)) == NULL)
	return NULL;
    xn->xn_type = XML_ATTRIBUTE;
    return xn;
}

/*
 * Create a body with a text.
 * return xml_node object
 */
struct xml_node *
xml_new_body(struct xml_node *xp, char *value)
{
    struct xml_node *xb;

    if ((xb = xml_new("body", xp)) != NULL){
	xb->xn_type = XML_BODY;
	xb->xn_value = strdup(value);
    }
    return xb;
}

/*! Find an XML node matching name among a parent's children.
 *
 * Get first XML node directly under xn_parent in the xml hierarchy with
 * name "name".
 *
 * @param[in]  xn_parent  Base XML object
 * @param[in]  name       shell wildcard pattern to match with node name
 *
 * @retval xmlobj        if found.
 * @retval NULL          if no such node found.
 */
struct xml_node *
xml_find(struct xml_node *xn_parent, char *name)
{
    int i;

    for (i=0; i<xn_parent->xn_nrchildren; i++)
	if (strcmp(name, xn_parent->xn_children[i]->xn_name) == 0)
	    return xn_parent->xn_children[i];
    return NULL;
}

/*
 * xml_each
 * iterator over xml children objects
 * xprev should be initialized to zero.
 * type selects only that type of child. -1 means all types
 * NOTE: Never manipulate the child-list during operation, the function
 * uses a kludge by remembering the index used. It works as long as the same
 * object is not iterated concurrently.
 *
 * Example of usage:
 *   struct xml_node *x = NULL;
 *   while ((x = xml_child_each(xn_top, x, -1)) != NULL) {
 *     ...
 *   }
 */
#if 0 /* slow, original version */
struct xml_node *
xml_child_each(struct xml_node *xparent, 
	       struct xml_node *xprev, 
	       enum node_type type)
{
    int i;
    struct xml_node *xn = NULL; 
    int next = 0;

    for (i=0; i<xparent->xn_nrchildren; i++){
	xn = xparent->xn_children[i];
	if (xn == NULL)
	    continue;
	if (type != XML_ERROR && xn->xn_type != type)
	    continue;
	if (xprev == NULL)
	    break; /* return first matching object */
	if (next)
	    break; /* this is next object after previous */
	if (xprev == xn)
	    next++;
    }
    if (i < xparent->xn_nrchildren){ /* found */
	xn->_xn_vector_i = i;
	return xn;
    }
    return NULL;
}
#else

struct xml_node *
xml_child_each(struct xml_node *xparent, 
	       struct xml_node *xprev, 
	       enum node_type type)
{
    int i;
    struct xml_node *xn = NULL; 

    for (i=xprev?xprev->_xn_vector_i+1:0; i<xparent->xn_nrchildren; i++){
	xn = xparent->xn_children[i];
	if (xn == NULL)
	    continue;
	if (type != XML_ERROR && xn->xn_type != type)
	    continue;
	break; /* this is next object after previous */
    }
    if (i < xparent->xn_nrchildren) /* found */
	xn->_xn_vector_i = i;
    else
	xn = NULL;
    return xn;
}
#endif

/*
 * xml_addsub
 * Add xc as child to xp. and remove xc from previous parent.
 */
int
xml_addsub(struct xml_node *xp, struct xml_node *xc)
{
    xp->xn_nrchildren++;
    xp->xn_children = realloc(xp->xn_children,
			      xp->xn_nrchildren*sizeof(struct xml_node*));
    if (xp->xn_children == NULL){
	clicon_err(OE_XML, errno, "%s: realloc", __FUNCTION__);
	return -1;
    }
    if (xc->xn_parent)
	xml_prune(xc->xn_parent, xc, 0);
    xp->xn_children[xp->xn_nrchildren-1] = xc;
    xc->xn_parent = xp; 
    return 0;
}

/*
 * xml_insert
 * Insert a new element (xc) under an xml node (xp), and move all original children
 * of the top to xc.
 * return the child (xc)
 */
struct xml_node *
xml_insert(struct xml_node *xp, char *tag)
{
    struct xml_node *xc; /* new child */

    if ((xc = xml_new(tag, NULL)) == NULL)
	goto catch;
    while (xp->xn_nrchildren)
	if (xml_addsub(xc, xp->xn_children[0]) < 0)
	    goto catch;
    if (xml_addsub(xp, xc) < 0)
	goto catch;
  catch:
    return xc;
}



/*
 * Similar to xml_find, but return the value field of the xml node
 * attribute instead. 
 * Note, the returned value is a pointer into the xml node hierarchy.
 * So you should usually copy the value to other storage.
 * Return NULL if no such attribute.
 * E.g.  xn_parent->attribute->value
 */
char *
xml_get(struct xml_node *xn_parent, char *name)
{
    struct xml_node *xn;
    
    if ((xn = xml_find(xn_parent, name)) != NULL)
	return xn->xn_value;
    return NULL;
}


/*
 * Variant of xml_get but with type value of int.
 * NOTE, 
 *  - if attribute does not exist, return -1!!
 * This means you must check before calling the function
 * that the attribute actually exists!
 */
int 
xml_get_int(struct xml_node *xn_parent, char *name)
{
    char *v, *ep;
    long i;

    if ((v = xml_get(xn_parent, name)) == NULL)
	return -1;
    errno = 0;
    i = strtol(v, &ep, 0);
    if ((i == LONG_MIN || i == LONG_MAX) && errno)
	return -1;
    return i;
}


#ifdef notused
/*
 * Similar to xml_get, but return the value of the attribute of the
 * a child instead.
 * E.g.  xn_parent->xn->attribute->value
 * XXX: make stdarg list of xml_get instead.
 */
char *
xml_get2(struct xml_node *xn_parent, char *name1, char *name2)
{
    struct xml_node *xn, *xa;
    
    if ((xn = xml_find(xn_parent, name1)) != NULL)
	if ((xa = xml_find(xn, name2)) != NULL)
	  return xa->xn_value;
    return NULL;
}
#endif

/*
 * xml_get_body
 * Given an xml node, go through its children and find the first
 * BODY and return its string value. This value is malloced and must
 * be copied in order to be used outside of the calling scope.
 */
char *
xml_get_body(struct xml_node *xn)
{
    int i;
    struct xml_node *xb;

    for (i=0; i<xn->xn_nrchildren; i++){
	xb = xn->xn_children[i];
	if (xb->xn_type == XML_BODY)
	    return xb->xn_value;
    }
    return NULL;
}

/*
 * xml_get_body2
 * Similar to xml_get_body, but return the body of
 * a child instead.
 * E.g.  xn_parent->xn->body->value
 */
char *
xml_get_body2(struct xml_node *xn, char *name)
{
    struct xml_node *x;

    if ((x = xml_find(xn, name)) != NULL)
	return xml_get_body(x);
    return NULL;
}

#ifdef notused
/*
 * Set the value of an attribute following a path down the tree.
 * The path points out a path through the tree like this 
 *      xn0->xn->xa
 *  xn0  - the root node
 *  name - the name of the sub-node xn. Create if it does not exist.
 *  attrname - the name of an attribute xa of xn. Create if it does not exist.
 *  value - the value of attribute xa.
 * Return 0 on success, -1 on failure.
 * XXX: could make into variable arg list, with many xn node.
 */
int
xml_put2(struct xml_node *xn0, char *name, char *attrname, char *value)
{
    struct xml_node *xn;

    if (xn0 == NULL){ /* Assertion: just to make calling node cleaner */
	clicon_err(OE_XML, errno, "%s: root nod NULL", __FUNCTION__);
	return -1;
    }
    if ((xn = xml_find(xn0, name)) == NULL)
	if ((xn = xml_new(name, xn0)) == NULL)
	    return -1;
    return xml_put(xn, attrname, value);
}

/* 
 * Variant of xml_put with int type value
 */
int
xml_put2_int(struct xml_node *xn0, char *name, char *attrname, int value)
{
    struct xml_node *xn;

    if (xn0 == NULL){ /* Assertion: just to make calling node cleaner */
	clicon_err(OE_XML, errno, "%s: root nod NULL", __FUNCTION__);
	return -1;
    }
    if ((xn = xml_find(xn0, name)) == NULL)
	if ((xn = xml_new(name, xn0)) == NULL)
	    return -1;
    return xml_put_int(xn, attrname, value);
}
#endif /* notused */

/* 
 * Variant of xml_put with only one step.
 */
int
xml_put(struct xml_node *xn, char *attrname, char *value)
{
    struct xml_node *xa;

    assert(xn);
    if ((xa = xml_find(xn, attrname)) == NULL)
	if ((xa = xml_new_attribute(attrname, xn)) == NULL)
	    return -1;
    if (xa->xn_type != XML_ATTRIBUTE){
	clicon_err(OE_XML, errno, "%s: %s not attribute", __FUNCTION__, attrname);
	return -1;
    }
    if (xa->xn_value){
	free(xa->xn_value);
	xa->xn_value = NULL;
    }
    if (value)
	xa->xn_value = strdup(value);
    return 0;
}

/* 
 * Variant of xml_put with int type value
 */
int
xml_put_int(struct xml_node *xn, char *attrname, int value)
{
    struct xml_node *xa;

    assert(xn);
    if ((xa = xml_find(xn, attrname)) == NULL)
	if ((xa = xml_new_attribute(attrname, xn)) == NULL)
	    return -1;
    if (xa->xn_type != XML_ATTRIBUTE){
	clicon_err(OE_XML, errno, "%s: %s not attribiute", __FUNCTION__, attrname);
	return -1;
    }
    if (xa->xn_value){
	free(xa->xn_value);
	xa->xn_value = NULL;
    }
    if ((xa->xn_value = malloc(32)) == NULL){
	clicon_err(OE_XML, errno, "%s: malloc", __FUNCTION__);
	return -1;
    }
    snprintf(xa->xn_value, 32, "%d", value);
    return 0;
}

/* 
 * Variant of xml_put with int type value
 */
int
xml_put_int64(struct xml_node *xn, char *attrname, 
		     long long unsigned int value)
{
    struct xml_node *xa;

    assert(xn);
    if ((xa = xml_find(xn, attrname)) == NULL)
	if ((xa = xml_new_attribute(attrname, xn)) == NULL)
	    return -1;
    if (xa->xn_type != XML_ATTRIBUTE){
	clicon_err(OE_XML, errno, "%s: %s not attribite", __FUNCTION__, attrname);
	return -1;
    }
    if (xa->xn_value){
	free(xa->xn_value);
	xa->xn_value = NULL;
    }
    if ((xa->xn_value = malloc(64)) == NULL){
	clicon_err(OE_XML, errno, "%s: malloc", __FUNCTION__);
	return -1;
    }
    snprintf(xa->xn_value, 64, "0x%llx", value);
    return 0;
}


/*
 * xml_prune.
 * Remove a node xc from a parent xp. (xc must be child of xp)
 * Differs from xml_free in two ways:
 *  1. It is removed from parent.
 *  2. If you set the freeit flag to 0, the cild tree will not be freed 
 *     (otherwise it will)
 */
int
xml_prune(struct xml_node *xp, struct xml_node *xc, int freeit)
{
    int i;

    for (i=0; i<xp->xn_nrchildren; i++)
	if (xc == xp->xn_children[i]){
	    xp->xn_children[i] = NULL;
	    xc->xn_parent = NULL;
	    if (freeit)
		xml_free(xc);	    
	    xp->xn_nrchildren--;
	    for (;i<xp->xn_nrchildren; i++)
		xp->xn_children[i] = xp->xn_children[i+1];
	    return 0;
	}
    clicon_err(OE_XML, errno, "%s:  xc (%s) not child of xp (%s)",
	    __FUNCTION__,  xc->xn_name, xp->xn_name);
    return -1; /* xc not child of xp */
}


/*
 * xml_free
 * Remove a node xn from parent. And free it, recursively.
 * Differs from xml_prune in that it is _not_ removed from parent.
 */
int
xml_free(struct xml_node *xn)
{
    struct xml_node *xn_child;
    int i;

    for (i=0; i<xn->xn_nrchildren; i++){
	xn_child = xn->xn_children[i];
	xml_free(xn_child);
	xn_child = NULL;
    }
    if (xn->xn_children)
	free(xn->xn_children);
    if (xn->xn_value)
	free(xn->xn_value);
    if (xn->xn_namespace)
	free(xn->xn_namespace);
    free(xn);
    return 0;
}

/*! Print an XML tree structure to an output stream
 *
 * See also xml_to_string
 *
 * @param[in]   f           UNIX output stream
 * @param[in]   xn          xmlgen xml tree
 * @param[in]   level       how many spaces to insert before each line
 * @param[in]   prettyprint insert \n and spaces tomake the xml more readable.
 */
int
xml_to_file(FILE *f, struct xml_node *xn, int level, int prettyprint)
{
    int i;
    struct xml_node *xn_child;
    int empty = 0;
    int body;
    int subnodes = 0;

    if (xn == NULL)
	return 0;
    switch(xn->xn_type){
    case XML_ATTRIBUTE:
	fprintf(f, " ");
	if (xn->xn_namespace)
	    fprintf(f, "%s:", xn->xn_namespace);
	fprintf(f, "%s=\"%s\"", xn->xn_name, xn->xn_value);
	break;
    case XML_BODY:
	/* The following is required in netconf but destroys xml pretty-printing
	   in other code */
#if 1
	fprintf(f, "%s", xn->xn_value);
#endif
	break;
    case XML_ELEMENT:
	fprintf(f, "%*s", prettyprint?(level+1):0, "<");
	if (xn->xn_namespace)
	    fprintf(f, "%s:", xn->xn_namespace);
	fprintf(f, "%s", xn->xn_name);
	/* Two passes: first attributes */
	for (i=0; i<xn->xn_nrchildren; i++){
	    xn_child = xn->xn_children[i];
	    if (xn_child->xn_type != XML_ATTRIBUTE){
		subnodes++;
		continue;
	    }
	    xml_to_file(f, xn_child, prettyprint?(level+2):0, prettyprint);
	}
	body = xn->xn_nrchildren && xn->xn_children[0]->xn_type==XML_BODY;
	if (prettyprint)
	    fprintf(f, ">%s", body?"":"\n");
	else
	    fprintf(f, ">"); /* No CR / want no xtra chars after end-tag */
	/* then nodes */
	if (!empty){
	    for (i=0; i<xn->xn_nrchildren; i++){
		xn_child = xn->xn_children[i];
		if (xn_child->xn_type == XML_ATTRIBUTE)
		    continue;
		else
		    xml_to_file(f, xn_child, level+2, prettyprint);
		
	    }
	    fprintf(f, "%*s%s>%s", 
		    (prettyprint&&!body)?(level+2):0, 
		    "</", 
		    xn->xn_name,
		    prettyprint?"\n":""
		);
	}
	break;
    default:
	break;
    }/* switch */
    return 0;
}

/*! Print an XML tree structure to a string
 *
 * @param[in,out] str         String to write to
 * @param[in]     xn          xmlgen xml tree
 * @param[in]     level       how many spaces to insert before each line
 * @param[in]     prettyprint insert \n and spaces tomake the xml more readable.
 * @param[in]     label       string used for chunk allocation, typically __FUNCTION__
 *
 * str is freed using (for example) unchunk_group(__FUNCTION__);
 * See also xml_to_file
 */
char *
xml_to_string(char *str,
	      struct xml_node *xn, 
	      int level, 
	      int prettyprint, 
	      const char *label)
{
    int i;
    struct xml_node *xn_child;
    int empty = 0;
    int body;
    int subnodes = 0;

    if (xn == NULL)
	return 0;
    switch(xn->xn_type){
    case XML_ATTRIBUTE:
	str = chunk_sprintf(label, "%s ", str);
	if (xn->xn_namespace)
	    str = chunk_sprintf(label, "%s%s:", str, xn->xn_namespace);
	str = chunk_sprintf(label, "%s%s=\"%s\"", str, xn->xn_name, xn->xn_value);
	break;
    case XML_BODY:
	/* The following is required in netconf but destroys xml pretty-printing
	   in other code */
#if 1
	str = chunk_sprintf(label, "%s%s", str, xn->xn_value);
#endif
	break;
    case XML_ELEMENT:
	str = chunk_sprintf(label, "%s%*s", str, prettyprint?(level+1):0, "<");
	if (xn->xn_namespace)
	    str = chunk_sprintf(label, "%s%s:", str, xn->xn_namespace);
	str = chunk_sprintf(label, "%s%s", str, xn->xn_name);
	/* Two passes: first attributes */
	for (i=0; i<xn->xn_nrchildren; i++){
	    xn_child = xn->xn_children[i];
	    if (xn_child->xn_type != XML_ATTRIBUTE){
		subnodes++;
		continue;
	    }
	    str = xml_to_string(str, xn_child, prettyprint?(level+2):0, prettyprint, 
			  label);
	}
	body = xn->xn_nrchildren && xn->xn_children[0]->xn_type==XML_BODY;
	if (prettyprint)
	    str = chunk_sprintf(label, "%s>%s", str, body?"":"\n");
	else
	    str = chunk_sprintf(label, "%s>", str); /* No CR / want no xtra chars after end-tag */
	/* then nodes */
	if (!empty){
	    for (i=0; i<xn->xn_nrchildren; i++){
		xn_child = xn->xn_children[i];
		if (xn_child->xn_type == XML_ATTRIBUTE)
		    continue;
		else
		    str = xml_to_string(str, xn_child, level+2, prettyprint, label);
		
	    }
	    str = chunk_sprintf(label, "%s%*s%s>%s", 
			  str,
			  (prettyprint&&!body)?(level+2):0, 
			  "</", 
			  xn->xn_name,
			  prettyprint?"\n":""
		);
	}
	break;
    default:
	break;
    }/* switch */
    return str;
}

/*
 * print_xml_xf_node
 * prettyprint - insert \n and spaces tomake the xml more readable.
 * This variant places the parser tree in an xf buffer (essentially as string)
 * Usage:
 * xf_t *xf;
 * xf = xf_alloc();
 * xf = print_xml_xf_node(xf, xn, 0, 1);
 * xf_free(xf);
 * See also xml_to_string
 */
int
print_xml_xf_node(xf_t *xf, struct xml_node *xn, int level, int prettyprint)
{
    int i;
    struct xml_node *xn_child;
    int empty = 0;
    int body;
    int subnodes = 0;

    switch(xn->xn_type){
    case XML_ATTRIBUTE:
	xprintf(xf, " ");
	if (xn->xn_namespace)
	    xprintf(xf, "%s:", xn->xn_namespace);
	xprintf(xf, "%s=\"%s\"", xn->xn_name, xn->xn_value);
	break;
    case XML_BODY:
	/* The following is required in netconf but destroys xml pretty-printing
	   in other code */
#if 1
	xprintf(xf, "%s", xn->xn_value);
#endif
	break;
    case XML_ELEMENT:
	xprintf(xf, "%*s", prettyprint?(level+1):0, "<");
	if (xn->xn_namespace)
	    xprintf(xf, "%s:", xn->xn_namespace);
	xprintf(xf, "%s", xn->xn_name);
	/* Two passes: first attributes */
	for (i=0; i<xn->xn_nrchildren; i++){
	    xn_child = xn->xn_children[i];
	    if (xn_child->xn_type != XML_ATTRIBUTE){
		subnodes++;
		continue;
	    }
	    print_xml_xf_node(xf, xn_child, prettyprint?(level+2):0, prettyprint);
	}
	body = xn->xn_nrchildren && xn->xn_children[0]->xn_type==XML_BODY;
	if (prettyprint)
	    xprintf(xf, ">%s", body?"":"\n");
	else
	    xprintf(xf, ">"); /* No CR / want no xtra chars after end-tag */
	/* then nodes */
	if (!empty){
	    for (i=0; i<xn->xn_nrchildren; i++){
		xn_child = xn->xn_children[i];
		if (xn_child->xn_type == XML_ATTRIBUTE)
		    continue;
		else
		    print_xml_xf_node(xf, xn_child, level+2, prettyprint);
	    }
	    xprintf(xf, "%*s%s>%s", 
		    (prettyprint&&!body)?(level+2):0, 
		    "</", 
		    xn->xn_name,
		    prettyprint?"\n":""
		);
	}
	break;
    default:
	break;
    }/* switch */
    return 0;
}



int 
xml_parse(char **xml_str, struct xml_node *xn_parent, char *dtd_file,
	  char *dtd_root_label, size_t dtd_len)
{
    char *str;
    int retval;
    struct xml_parse_yacc_arg ya = {0,};

    if ((str = strdup(*xml_str)) == NULL){
	clicon_err(OE_XML, errno, "%s: strdup", __FUNCTION__);
	return -1;
    }
    ya.ya_parse_string = str;
    if (xmll_init(&ya) < 0){
      	free(str);
	return -1;
    }
    if (xmly_init(xn_parent, "XML parse") < 0){
      	free (str);
	return -1;
    }
    if ((retval = xmlparse(&ya)) != 0) {
	free (str);
	return -1;
    }
    free (str);
    xmll_exit(&ya);
    return 0;
}

/*! Read an XML definition from file and parse it into a parse-tree. 
 *
 * @param fd       A file descriptor containing the XML file (as ASCII characters)
 * @param xml_top  Pointer to an (on entry empty) pointer to an XML parse tree 
 *                 _created_ by this function.
 * @param  eof     If true, fd is at eof after reading the XML definition.
 * @param  endtag  Read until you encounter "endtag" in the stream
 *
 * Note, you need to free the xml parse tree after use, using xml_free()
 * Returns: 0 on sucess, -1 on failure.
 * XXX: There is a potential leak here on some return values.
 */
int 
xml_parse_fd(int fd, struct xml_node **xml_top, int *eof, char *endtag)
{
    int len = 0;
    char ch;
    int retval;
    char *xmlbuf, *ptr;
    int maxbuf = XML_PARSE_BUFLEN;
    int endtaglen = strlen(endtag);
    int state = 0;

    if (endtag == NULL){
	clicon_err(OE_XML, 0, "%s: endtag required\n", __FUNCTION__);
	return -1;
    }
    *xml_top = NULL;
    if ((xmlbuf = malloc(maxbuf)) == NULL){
	clicon_err(OE_XML, errno, "%s: malloc", __FUNCTION__);
	return -1;
    }
    memset(xmlbuf, 0, maxbuf);
    ptr = xmlbuf;
    if (xdebug)
	fprintf(stderr, "%s:\n", __FUNCTION__);
    while (1){
	retval = read(fd, &ch, 1);
	if (retval < 0){
	    clicon_err(OE_XML, errno, "%s: read: [pid:%d]\n", 
		    __FUNCTION__,
		    (int)getpid());
	    break;
	}
	if (retval == 0){
	    if (eof)
		*eof = 1;
	}
	else{
	    if (xdebug)
		fputc(ch, stderr);
	    state = statestr(endtag, ch, state);
	    xmlbuf[len++] = ch;
	}
	if (retval == 0 || state == endtaglen){
	    state = 0;
	    if ((*xml_top = xml_new("top", NULL)) == NULL)
		break;

	    if (xml_parse(&ptr, *xml_top, NULL, NULL, 0) < 0)
		return -1;


	    break;
	}
	if (len>=maxbuf-1){ /* Space: one for the null character */
	    int oldmaxbuf = maxbuf;

	    maxbuf *= 2;
	    if ((xmlbuf = realloc(xmlbuf, maxbuf)) == NULL){
		clicon_err(OE_XML, errno, "%s: realloc", __FUNCTION__);
		return -1;
	    }
	    memset(xmlbuf+oldmaxbuf, 0, maxbuf-oldmaxbuf);
	    ptr = xmlbuf;
	}
    } /* while */
    free(xmlbuf);
    return (*xml_top)?0:-1;
}


/*! Read an XML definition from string and parse it into a parse-tree. 
 *
 * @code
 *  str = strdup(...);
 *  str0 = str;
 *  xml_parse_str(&str, &xml_node)
 *  free(str0);
 *  xml_free(xml_node);
 * @endcode
 *
 * @param str   Pointer to string containing XML definition. NOTE: destructively
 *          modified. This means if str is malloced, you need tomake a copy
 *          of str before use and free that. 
 *
 * @param  xml_top Top of the XML parse tree created by this function.
 *
 * Note, you need to free the xml parse tree after use, using xml_free()
 * Update: with yacc parser I dont think it changes,....
 */
int 
xml_parse_str(char **str, struct xml_node **xml_top)
{
  if ((*xml_top = xml_new("top", NULL)) == NULL)
    return -1;
  return xml_parse(str, *xml_top, NULL, NULL, 0);
}

/*
 * xml_cp1
 * Copy xml node from xn0 to xn1 (non-recursive)
 */
int
xml_cp1(struct xml_node *xn0, struct xml_node *xn1)
{
    xn1->xn_type = xn0->xn_type;
    if (xn0->xn_value) /* malloced string */
	xn1->xn_value = strdup(xn0->xn_value);
    return 0;
}

/*! Copy xml tree to other existing tree
 *
 * x1 should be a created placeholder. If x1 is non-empty,
 * the copied tree is appended to the existing tree.
 * @code
 *   x1 = xml_new("new", xc);
 *   xml_cp(x0, x1);
 * @endcode
 */
int
xml_cp(struct xml_node *x0, struct xml_node *x1)
{
    int i;
    struct xml_node *xc0;
    struct xml_node *xc1;

    assert(x0 && x1);
    xml_cp1(x0, x1);
    for (i=0; i<x0->xn_nrchildren; i++){
	xc0 = x0->xn_children[i];
	if ((xc1 = xml_new(xc0->xn_name, x1)) == NULL)
	    return -1;
	if (xml_cp(xc0, xc1) < 0)
	    return -1;
    }
    return 0;
}

/*! Create and return a copy of xml tree.
 *
 * @code
 *   struct xml_node *x1;
 *   x1 = xml_dup(x0);
 * @endcode
 * Note, returned tree should be freed as: xml_free(x1)
 */
struct xml_node *
xml_dup(struct xml_node *x0)
{
    struct xml_node *x1;

    if ((x1 = xml_new("new", NULL)) == NULL)
	return NULL;
    if (xml_cp(x0, x1) < 0)
	return NULL;
    return x1;
}

/*
 * xml_node_eq
 * Two xml nodes are node-equal if they have the same:
 * (1) type, name and value. (value can be NULL or string)
 * (2) same set children in terms of names
 * Return 1 if equal, 0 if not.
 */
int 
xml_node_eq(struct xml_node *x0, struct xml_node *x1)
{
    struct xml_node *x0c, *x1c;
    int eq = 0; /* not equal */

//    fprintf(stderr, "%s: %s %s\n", __FUNCTION__, x0->xn_name, x1->xn_name);
    if (x0->xn_type != x1->xn_type)
	goto noteq;
    if (strcmp(x0->xn_name, x1->xn_name))
	goto noteq;
    if (x0->xn_value != x0->xn_value)
	goto noteq;
    if (x0->xn_value && x1->xn_value && strcmp(x0->xn_value, x1->xn_value))
	goto noteq;
    x0c = NULL; /* Check if x0's children match x1 */
    while ((x0c = xml_child_each(x0, x0c, -1)) != NULL)
	if (xml_find(x1, x0c->xn_name) == NULL)
	    goto noteq;
    x1c = NULL; /* Check if x0's children match x1 */
    while ((x1c = xml_child_each(x1, x1c, -1)) != NULL)
	if (xml_find(x0, x1c->xn_name) == NULL)
	    goto noteq;
    eq = 1; /* equal */
  noteq:
    return eq;
}

/*
 * xml_eq
 * Two xml nodes are equal if they (1-2) are node equal
 * (1) type, name and value. (value can be NULL or string)
 * (2) same set children in terms of names
 * (3) the children with the same names are equal
 */
int 
xml_eq(struct xml_node *x0, struct xml_node *x1)
{
    struct xml_node *x0c, *x1c;
    int eq = 0;
    
    if (!xml_node_eq(x0, x1))
	goto neq;
    x0c = NULL; /* Check if x0's children match x1 */
    while ((x0c = xml_child_each(x0, x0c, -1)) != NULL){
	if ((x1c = xml_find(x0, x0c->xn_name)) == NULL)
	    goto neq; /* shouldnt happen */
	if (!xml_eq(x0c, x1c))
	    goto neq;
    }
    eq = 1;
  neq:
    return eq;
}

/*
 * xml_delete
 * XML nodes in xn that are found in xc are deleted from xc.
 * More specifically, the nods should be equal, and the node in xn should not have children.
 */
int
xml_delete(struct xml_node *xc, struct xml_node *xn,
	   int fn(struct xml_node *, struct xml_node *))
{
    struct xml_node *xcc; /* configuration child */
    struct xml_node *xnc; /* new child */
    int i, j;
    int subelements;

//    fprintf(stderr, "%s: %s %s\n", __FUNCTION__, xc->xn_name, xn->xn_name);
    for (i=0; i<xn->xn_nrchildren; i++){
	xnc = xn->xn_children[i];
	if (xnc->xn_type != XML_ELEMENT)
	    continue;
//	fprintf(stderr, "%s: Trying %s->%s\n", __FUNCTION__, xn->xn_name, xnc->xn_name);

	/* See if xnc is stub (no children) */
	subelements = 0;
	for (j=0; j<xnc->xn_nrchildren; j++)
	    if (xnc->xn_children[j]->xn_type == XML_ELEMENT)
		subelements++;
	/* Try to find node with equal element + attributes */
	xcc = NULL;
	for (j=0; j<xc->xn_nrchildren; j++){
	    xcc = xc->xn_children[j];
	    if (xml_node_eq(xnc, xcc))
		break;
	}
	if (j==xc->xn_nrchildren) /* Did not find equal node in xc */
	    continue;
	assert(xcc!= NULL);
	/* xcc & xnc are equal */
//	fprintf(stderr, "%s: Equal %s->%s subel:%d\n", __FUNCTION__, xn->xn_name, xnc->xn_name, subelements);
	if (subelements == 0){ 	/* filter has no children -> remove in config */
//	    fprintf(stderr, "%s: Remove %s->%s \n", __FUNCTION__, xc->xn_name, xcc->xn_name);
	    xml_prune(xc, xcc, 1); /* remove it */
	}
	else
	    if (xml_delete(xcc, xnc, fn) < 0) /* Examine children */
		return -1;
    }
    return 0;
}

