/*
 *  CVS Version: $Id: xmlgen_xml.h,v 1.11 2013/08/17 10:54:22 benny Exp $
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

 * XML support functions.
 */
#ifndef _XMLGEN_XML_H
#define _XMLGEN_XML_H

/*
 * Constants
 */
#define XML_PARSE_BUFLEN 1024  /* Size of xml read buffer */
#define XML_FLEN 64

/*
 * Types
 */
enum node_type {XML_ERROR=-1, XML_ELEMENT, XML_ATTRIBUTE, XML_BODY};

struct xml_node{
    char            xn_name[XML_FLEN];
    enum node_type  xn_type; 
    char            *xn_value;  /* malloced */
    struct xml_node *xn_parent;
    struct xml_node **xn_children;
    int             xn_nrchildren;
    char            *xn_namespace; /* if != NULL, name of namespace */
    int              xn_index;     /* unique, aka sql index */
    int              _xn_vector_i; /* internal use: xml_child_each */
};
typedef struct xml_node xml_node_t;

/*
 * XML debug variable. can be set in several steps
 * with increased debug detail.
 */
extern int xdebug; /* XXX: remove */

/*
 * Prototypes
 */
int   xml_debug(int value);
char *xml_decode_attr_value(char *val);
struct xml_node *xml_new(char *name, struct xml_node *xn_parent);
struct xml_node *xml_new_attribute(char *name, struct xml_node *xn_parent);
struct xml_node *xml_new_body(struct xml_node *xp, char *value);

struct xml_node *xml_find(struct xml_node *xn_parent, char *name);

struct xml_node *xml_child_each(struct xml_node *xparent, 
				struct xml_node *xprev,  enum node_type type);

char *xml_get(struct xml_node *xn_parent, char *name);
int   xml_get_int(struct xml_node *xn_parent, char *name);
char *xml_get_body(struct xml_node *xn);
char *xml_get_body2(struct xml_node *xn, char *name);

int xml_put(struct xml_node *xn, char *attrname, char *value);
int xml_put_int(struct xml_node *xn, char *attrname, int value);
int xml_put_int64(struct xml_node *xn, char *attrname, long long unsigned int value);

int xml_free(struct xml_node *xn);
int xml_prune(struct xml_node *xp, struct xml_node *xc, int freeit);
int xml_to_file(FILE *f, struct xml_node *xn, int level, int prettyprint);
char *xml_to_string(char *str,  struct xml_node *xn, 
		    int level, int prettyprint, const char *label);
int xml_parse(char **str, struct xml_node *xn_parent, char *dtd_file,
	      char *dtd_root_label, size_t dtd_len);
int xml_parse_fd(int fd, struct xml_node **xml_top, int *eof, char *endtag);
int xml_parse_str(char **str, struct xml_node **xml_top);

int xml_cp1(struct xml_node *xn0, struct xml_node *xn1);
int xml_cp(struct xml_node *xn0, struct xml_node *xn1);
int xml_node_eq(struct xml_node *x0, struct xml_node *x1);
int xml_eq(struct xml_node *x0, struct xml_node *x1);
int xml_delete(struct xml_node *xc, struct xml_node *xn,
	       int fn(struct xml_node *, struct xml_node *));
int xml_addsub(struct xml_node *xp, struct xml_node *xc);
struct xml_node *xml_insert(struct xml_node *xt, char *tag);

#endif /* _XMLGEN_XML_H */
