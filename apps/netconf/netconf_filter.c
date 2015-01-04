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
 *  netconf match & selection: get and edit operations
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <assert.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include <clicon/clicon.h>

#include "netconf_rpc.h"
#include "netconf_lib.h"
#include "netconf_filter.h"

/* forward */
static int
xml_edit(cxobj *filter, 
	 cxobj *parent, 
	 enum operation_type op,
	 cbuf *xf_err, 
	 cxobj *xt);


/*
 * xml_filter
 * xf specifices a filter, and xn is an xml tree.
 * Select the part of xn that matches xf and return it.
 * Change xn destructively by removing the parts of the sub-tree that does 
 * not match.
 * Match according to Section 6 of RFC 4741.
    NO_FILTER,       select all 
    EMPTY_FILTER,    select nothing 
    ATTRIBUTE_MATCH, select if attribute match 
    SELECTION,       select this node 
    CONTENT_MATCH,   select all siblings with matching content 
    CONTAINMENT      select 
 */

/* return a string containing leafs value, NULL if no leaf or no value */
static char*
leafstring(cxobj *x)
{
    cxobj *c;

    if (xml_type(x) != CX_ELMNT)
	return NULL;
    if (xml_child_nr(x) != 1)
	return NULL;
    c = xml_child_i(x, 0);
    if (xml_child_nr(c) != 0)
	return NULL;
    if (xml_type(c) != CX_BODY)
	return NULL;
    return xml_value(c);
}

/*
 * Select siblings of xp
 * assume parent has been selected and filter match (same name) as parent
 * Return values: 0 OK, -1 error
 * parent is pruned according to selection.
 */
static int
select_siblings(cxobj *filter, cxobj *parent, int *remove_me)
{
    cxobj *s;
    cxobj *sprev;
    cxobj *f;
    cxobj *attr;
    char *an, *af;
    char *fstr, *sstr;
    int containments;

    *remove_me = 0;
    assert(filter && parent && strcmp(xml_name(filter), xml_name(parent))==0);
    /* 1. Check selection */
    if (xml_child_nr(filter) == 0) 
	goto match;

    /* Count containment/selection nodes in filter */
    f = NULL;
    containments = 0;
    while ((f = xml_child_each(filter, f, CX_ELMNT)) != NULL) {
	if (leafstring(f))
	    continue;
	containments++;
    }

    /* 2. Check attribute match */
    attr = NULL;
    while ((attr = xml_child_each(filter, attr, CX_ATTR)) != NULL) {
	af = xml_value(attr);
	an = xml_find_value(filter, xml_name(attr));
	if (af && an && strcmp(af, an)==0)
	    ; // match
	else
	    goto nomatch;
    }
    /* 3. Check content match */
    f = NULL;
    while ((f = xml_child_each(filter, f, CX_ELMNT)) != NULL) {
	if ((fstr = leafstring(f)) == NULL)
	    continue;
	if ((s = xml_find(parent, xml_name(f))) == NULL)
	    goto nomatch;
	if ((sstr = leafstring(s)) == NULL)
	    continue;
	if (strcmp(fstr, sstr))
	    goto nomatch;
    }
    /* If filter has no further specifiers, accept */
    if (!containments)
	goto match;
    /* Check recursively the rest of the siblings */
    sprev = s = NULL;
    while ((s = xml_child_each(parent, s, CX_ELMNT)) != NULL) {
	if ((f = xml_find(filter, xml_name(s))) == NULL){
	    xml_prune(parent, s, 1);
	    s = sprev;
	    continue;
	}
	if (leafstring(f)){
	    sprev = s;
	    continue; // unsure?sk=lf
	}
	{ 	// XXX: s can be removed itself in the recursive call !
	    int remove_s = 0;
	    if (select_siblings(f, s, &remove_s) < 0)
		return -1;
	    if (remove_s){
		xml_prune(parent, s, 1);
		s = sprev;
	    }
	}
	sprev = s;
    }

  match:
    return 0;
  nomatch: /* prune this parent node (maybe only children?) */
    *remove_me = 1;
    return 0;
}

int
xml_filter(cxobj *xf, cxobj *xn)
{
    int retval;
    int remove_s;

    retval = select_siblings(xf, xn, &remove_s);
    return retval;
}

/*
 * get_operation
 * get the value of the "operation" attribute and change op if given
 */
static int
get_operation(cxobj *xn, enum operation_type *op,
	      cbuf *xf_err, cxobj *xt)
{
    char *opstr;

    if ((opstr = xml_find_value(xn, "operation")) != NULL){
	if (strcmp("merge", opstr) == 0)
	    *op = OP_MERGE;
	else
	if (strcmp("replace", opstr) == 0)
	    *op = OP_REPLACE;
	else
	if (strcmp("create", opstr) == 0)
	    *op = OP_CREATE;
	else
	if (strcmp("delete", opstr) == 0)
	    *op = OP_DELETE;
	else
	if (strcmp("remove", opstr) == 0)
	    *op = OP_REMOVE;
	else{
	    netconf_create_rpc_error(xf_err, xt, 
				     "bad-attribute", 
				     "protocol", 
				     "error", 
				     NULL,
				     "<bad-attribute>operation</bad-attribute>");
	    return -1;
	}
    }
    return 0;
}


/*
 * in edit_config, we copy a tree to the config. But some wthings shouldbe 
 * cleaned:
 * - operation attribute
 */
static int
netconf_clean(cxobj *xn)
{
    cxobj *xa;    
    cxobj *x;

    if ((xa = xml_find(xn, "operation")) != NULL &&
	xml_type(xa) == CX_ATTR)
	xml_prune(xn, xa, 1);
    x = NULL;
    while ((x = xml_child_each(xn, x, CX_ELMNT)) != NULL) 
	netconf_clean(x);
    return 0;
}

/*
 * xml_edit
 * Merge two XML trees according to RFC4741/Junos
 * 1. in configuration(parent) but not in new(filter) -> remain in configuration
 * 2. not in configuration but in new -> add to configuration
 * 3. Both in configuration and new: Do 1.2.3 with children.
 * A key is: the configuration data identified by the element
 */
static int
edit_selection(cxobj *filter, 
	       cxobj *parent, 
	       enum operation_type op,
	       cbuf *xf_err, 
	       cxobj *xt)
{
    int retval = -1;

    assert(filter && parent && strcmp(xml_name(filter), xml_name(parent))==0);
    fprintf(stderr, "%s: %s\n", __FUNCTION__, xml_name(filter));
    switch (op){
    case OP_MERGE:
	break;
    case OP_REPLACE:
	xml_prune(xml_parent(parent), parent, 1);
	break;
    case OP_CREATE:
	    netconf_create_rpc_error(xf_err, xt, 
				     NULL,
				     "protocol", 
				     "error", 
				     "statement creation failed",
				     "<bad-element></bad-element>");
	    goto done;
	break;
    case OP_DELETE:
	fprintf(stderr, "%s: %s DELETE\n", __FUNCTION__, xml_name(filter));
	if (xml_child_nr(parent) == 0){
	    fprintf(stderr, "%s: %s ERROR\n", __FUNCTION__, xml_name(filter));
	    netconf_create_rpc_error(xf_err, xt, 
				     NULL,
				     "protocol", 
				     "error", 
				     "statement not found",
				     "<bad-element></bad-element>");
	    goto done;
	}
	/* fall through */
    case OP_REMOVE:
	fprintf(stderr, "%s: %s REMOVE\n", __FUNCTION__, xml_name(filter));
	xml_prune(xml_parent(parent), parent, 1);
	break;
    case OP_NONE:
	break;
    }
    retval = 0;
    netconf_ok_set(1); /* maybe cc_ok shouldnt be set if we continue? */
  done:
    return retval;
}

/*
 * XXX: not called from external?
 */
static int
edit_match(cxobj *filter, 
	   cxobj *parent, 
	   enum operation_type op,
	   cbuf *xf_err, 
	   cxobj *xt,
	   int match
    )
{
    cxobj *f;
    cxobj *s;
    cxobj *copy;
    int retval = -1;

    clicon_debug(1, "%s: %s op:%d", __FUNCTION__, xml_name(filter), op);
    if (match)
	switch (op){
	case OP_REPLACE:
	case OP_CREATE:
	    s = NULL;
	    while ((s = xml_child_each(parent, s, -1)) != NULL){ 
		xml_prune(parent, s, 1);
		s = NULL;
	    }
	    if (xml_copy(filter, parent) < 0)
		goto done;
	    netconf_clean(parent);
	    retval = 0;
	    netconf_ok_set(1);
	    goto done;
	    break;
	case OP_DELETE:
	case OP_REMOVE:
	    xml_prune(xml_parent(parent), parent, 1);
	    netconf_ok_set(1);
	    goto done;
	    break;
	case OP_MERGE: 
	case OP_NONE:
	    break;
	}

    f = NULL;
    while ((f = xml_child_each(filter, f, CX_ELMNT)) != NULL) {
	s = xml_find(parent, xml_name(f));
	switch (op){
	case OP_MERGE: 
	    /* things in filter:
	       not in conf should be added 
	       in conf go down recursive
	    */
	    if (s == NULL && match){
		if ((copy = xml_new(xml_name(f), parent)) == NULL)
		    goto done;
		if (xml_copy(f, copy) < 0)
		    goto done;
		netconf_clean(copy);
	    }
	    else{
		s = NULL;
		while ((s = xml_child_each(parent, s, CX_ELMNT)) != NULL) {
		    if (strcmp(xml_name(f), xml_name(s)))
			continue;
		    if ((retval = xml_edit(f, s, op, xf_err, xt)) < 0)
			goto done;
		}
	    }
	    break;
	case OP_REPLACE:
#if 1
	    /* things in filter
	       in conf: remove from conf and
	       add to conf
	    */
//	    if (!match)
//		break;
	    if (s != NULL)
		xml_prune(parent, s, 1);
	    if ((copy = xml_new(xml_name(f), parent)) == NULL)
		goto done;
	    if (xml_copy(f, copy) < 0)
		goto done;
	    netconf_clean(copy);
#endif
	    break;
	case OP_CREATE:
#if 0
	    /* things in filter
	       in conf: error
	       else add to conf
	    */
	    if (!match)
		break;
	    if (s != NULL){
		netconf_create_rpc_error(xf_err, xt, 
					 NULL,
					 "protocol", 
					 "error", 
					 "statement creation failed",
					 "<bad-element></bad-element>");
		goto done;
	    }
	    if ((copy = xml_new(xml_name(f), parent)) == NULL)
		goto done;
	    if (xml_copy(f, copy) < 0)
		goto done;
	    netconf_clean(copy);
#endif
	    break;
	case OP_DELETE:
	    /* things in filter
	       if not in conf: error
	       else remove from conf
	    */
#if 0
	    if (!match)
		break;
	    if (s == NULL){
		netconf_create_rpc_error(xf_err, xt, 
					 NULL,
					 "protocol", 
					 "error", 
					 "statement not found",
					 "<bad-element></bad-element>");
		goto done;
	    }
#endif
	    /* fall through */
	case OP_REMOVE:
	    /* things in filter
	       remove from conf
	    */
#if 0
	    if (!match)
		break;
	    xml_prune(parent, s, 1);
#endif
	    break;
	case OP_NONE:
	    /* recursive descent */
	    s = NULL;
	    while ((s = xml_child_each(parent, s, CX_ELMNT)) != NULL) {
		if (strcmp(xml_name(f), xml_name(s)))
		    continue;
		if ((retval = xml_edit(f, s, op, xf_err, xt)) < 0)
		    goto done;
	    }
	    break;
	}
    } /* while f */
    retval = 0;
    netconf_ok_set(1); /* maybe cc_ok shouldnt be set if we continue? */
  done:
    return retval;
    
}

/*
 * xml_edit
 * merge filter into parent
 * XXX: not called from external?
 */
static int
xml_edit(cxobj *filter, 
	 cxobj *parent, 
	 enum operation_type op,
	 cbuf *xf_err, 
	 cxobj *xt)
{
    cxobj *attr;
    cxobj *f;
    cxobj *s;
    int retval = -1;
    char *an, *af;
    char *fstr, *sstr;
    int keymatch = 0;

    get_operation(filter, &op, xf_err, xt);
    /* 1. First try selection: filter is empty */
    if (xml_child_nr(filter) == 0){  /* no elements? */
	retval = edit_selection(filter, parent, op, xf_err, xt);
	goto done;
    }
    if (xml_child_nr(filter) == 1 && /* same as above */
	xpath_first(filter, "/[@operation]")){
	retval = edit_selection(filter, parent, op, xf_err, xt);
	goto done;
    }
    /* 2. Check attribute match */
    attr = NULL;
    while ((attr = xml_child_each(filter, attr, CX_ATTR)) != NULL) {
	af = xml_value(attr);
	an = xml_find_value(filter, xml_name(attr));
	if (af && an && strcmp(af, an)==0)
	    ; // match
	else
	    goto nomatch;
    }
    /* 3. Check content match */
    /*
     * For content-match we do a somewhat strange thing, we find
     * a match in first content-node and assume that is unique
     * and then we remove/replace that
     * For merge we just continue
     */
    f = NULL;
    while ((f = xml_child_each(filter, f, CX_ELMNT)) != NULL) {
	if ((fstr = leafstring(f)) == NULL)
	    continue;
	/* we found a filter leaf-match: no return we say it should match*/
	if ((s = xml_find(parent, xml_name(f))) == NULL)
	    goto nomatch;
	if ((sstr = leafstring(s)) == NULL)
	    goto nomatch;
	if (strcmp(fstr, sstr))
	    goto nomatch;
	keymatch++;
	break; /* match */
    }

    if (debug && keymatch){
	fprintf(stderr, "%s: match\n", __FUNCTION__);
	fprintf(stderr, "%s: filter:\n", __FUNCTION__);
	clicon_xml2file(stdout, filter, 0, 1);
	fprintf(stderr, "%s: config:\n", __FUNCTION__);
	clicon_xml2file(stdout, parent, 0, 1);
    }

    retval = edit_match(filter, parent, op, xf_err, xt, keymatch);
    /* match */
    netconf_ok_set(1);
    retval = 0;
  done:
    return retval;
  nomatch:
    return 0;
}

/*
 * netconf_xpath
 * Arguments:
 *  xsearch is where you search for xpath, grouped by a single top node which is
 *          not significant and will not be returned in any result. 
 *  xfilter is the xml sub-tree, eg: 
 *             <filter type="xpath" select="/t:top/t:users/t:user[t:name='fred']"/>
 *  xt is original tree
 *  
 */
int 
netconf_xpath(cxobj *xsearch,
	      cxobj *xfilter, 
	      cbuf *xf, 
	      cbuf *xf_err, 
	      cxobj *xt)
{
    cxobj  *x;
    int               retval = -1;
    char             *selector;
    cxobj **xv;
    int               xlen;
    int               i;

    if ((selector = xml_find_value(xfilter, "select")) == NULL){
	netconf_create_rpc_error(xf_err, xt, 
				 "operation-failed", 
				 "application", 
				 "error", 
				 NULL,
				 "select");
	goto done;
    }

    x = NULL;

    clicon_errno = 0;
    if ((xv = xpath_vec(xsearch, selector, &xlen)) != NULL) {
	for (i=0; i<xlen; i++){
	    x = xv[i];
	    clicon_xml2cbuf(xf, x, 0, 1);
	}
	free(xv);
    }
    /* XXX: NULL means error sometimes */
    if (clicon_errno){
	netconf_create_rpc_error(xf_err, xt, 
				 "operation-failed", 
				 "application", 
				 "error", 
				 clicon_err_reason,
				 "select");
	goto done;
    }

    retval = 0;
  done:
    return retval;
}


