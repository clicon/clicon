/*
 *  CVS Version: $Id: xmlgen_xf.h,v 1.4 2013/08/01 09:15:46 olof Exp $
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

/*
 * This module creates a wrapper around strings used for dynamic printing
 * of XML streams.
 * Example:
 * xf_t *xf;
 * xf = xf_alloc();
 * xprintf(xf, "<foo attr=\"%d\">\n", 17);
 * xprintf(xf, "</foo>");
 * write(f, xf->xf_buf, xf->xf_len);
 * xf_free(xf);
 */

#ifndef _XMLGEN_XF_H
#define _XMLGEN_XF_H

#include <stdarg.h> /* Include for ap below */

/*
 * Constants
 */
#define XF_BUFLEN 8*1024   /* Start length */


/*
 * Macros
 */
#define XML_HEADER(xf)  xprintf(xf, XML_HEADER_STR)
#define XML_TRAILER(xf)  xprintf(xf, XML_TRAILER_STR);	

/*
 * Types
 */
/* Wrapper around a string */
typedef struct {
    char  *xf_buf;     /* malloc'd string */
    size_t xf_maxbuf;  /* length of buf */
    size_t xf_len;     /* length of string (strlen is actually adequate) */
} xf_t;

/* xf variable context for xf_push/xf_pop */
typedef struct {
    xf_t *xc_res;
    xf_t *xc_err;
    int   xc_errno;
} xf_context;

/*
 * Prototypes
 */
struct xml_node; /* forward declaration */

xf_t *xf_alloc(void);
void xf_free(xf_t *xf);
int xprintf(xf_t *xf, const char *format, ...);
void xf_reset(xf_t *xf);
int print_xml_xf_node(xf_t *xf, struct xml_node *xn, int level, int prettyprint);
int xf_encode_attr(xf_t *xf);

#endif /* _XMLGEN_XF_H */
