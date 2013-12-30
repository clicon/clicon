/*
 *  CVS Version: $Id: xmlgen_xf.c,v 1.8 2013/08/17 10:54:22 benny Exp $
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmlgen_xf.h"

/* for debugging */
static int debug = 0;

/*
 * Allocate xml stream. The handle returned can be used in 
 * successive xprintf calls which dynamically print a string.
 * The handle should be freed by xmlf_free()
 * Return the allocated object handle on success.
 * Returns NULL on error.
 */
xf_t *
xf_alloc()
{
    xf_t *xf;

    if ((xf = (xf_t*)malloc(sizeof(*xf))) == NULL)
	return NULL;
    memset(xf, 0, sizeof(*xf));
    if ((xf->xf_buf = malloc(XF_BUFLEN)) == NULL)
	return NULL;
    xf->xf_maxbuf = XF_BUFLEN;
    memset(xf->xf_buf, 0, xf->xf_maxbuf);
    xf->xf_len = 0;
    return xf;
}

/*
 * Free xml stream previously created by xf_alloc
 */
void
xf_free(xf_t *xf)
{
    if (xf->xf_buf)
	free(xf->xf_buf);
    free(xf);
}

/*
 * Create XML string dynamically.
 * xf is an XML stream allocated by xf_alloc.
 * format and arguments uses printf syntax.
 */
int
xprintf(xf_t *xf, const char *format, ...)
{
    va_list ap;
    int retval;
    int remaining;

    va_start(ap, format);
  retry:
    if (xf == NULL)
	return 0;
    retval = vsnprintf(xf->xf_buf+xf->xf_len, 
		       xf->xf_maxbuf-xf->xf_len, 
		       format, ap);
    if (retval < 0)
	return -1;
    remaining = xf->xf_maxbuf - (xf->xf_len + retval + 1);
    if (remaining <= 0){
	if (debug)
	    fprintf(stderr, "xprintf: Reallocate %d -> %d\n", (int)xf->xf_maxbuf, 
		    (int)(2*xf->xf_maxbuf));
	xf->xf_maxbuf *= 2;
	if ((xf->xf_buf = realloc(xf->xf_buf, xf->xf_maxbuf)) == NULL)
	    return -1;
	va_end(ap);
	va_start(ap, format);
	goto retry;
    }
    xf->xf_len += retval;

    va_end(ap);
    return retval;
}


void
xf_reset(xf_t *xf)
{
    xf->xf_len=0; 
    xf->xf_buf[0] = '\0'; 
}

/*
 * xf_encode_attr
 * Translate entity references and return new allocated string.
 * This should only be applied to XML attribute values or XML bodies.
 * &lt;     <--     <
 * &gt;     <--     >
 * &amp;    <--     &
 * &apos;   <--     '
 * &quot;   <--     "
 */
int
xf_encode_attr(xf_t *xf)
{
  int i;
  char c;
  char escape[16];
  char *newbuf;
  int newlen;
  int newmax;
  int remaining;

  newlen = 0;
  newmax = xf->xf_maxbuf;
  if ((newbuf = malloc(newmax)) == NULL)
      return -1;
  memset(newbuf, 0, newmax);
  for (i=0; i<xf->xf_len; i++){
      c = xf->xf_buf[i];
      *escape = '\0';
      switch (c){ /* Find which escape sequence */
      case '<':
	  strncpy(escape, "&lt;", 15);
	  break;
      case '>':
	  strncpy(escape, "&gt;", 15);
	  break;
      case '&':
	  strncpy(escape, "&amp;", 15);
	  break;
      case '\'':
	  strncpy(escape, "&apos;", 15);
	  break;
      case '\"':
	  strncpy(escape, "&quot;", 15);
	  break;
      default: /* Regular character */
	  break;
      }
      if (strlen(escape)){
	retry:
	  remaining = newmax - (newlen + strlen(escape) + 1);
	  if (remaining <= 0){
	      if (debug)
		  fprintf(stderr, "%s: Reallocate %d -> %d\n", __FUNCTION__,
			  (int)newmax, 
			  (int)(2*newmax));
	      newmax *= 2;
	      if ((newbuf = realloc(newbuf, newmax)) == NULL)
		  return -1;
	      goto retry;
	  }
	  memcpy(&newbuf[newlen], escape, strlen(escape));
	  newlen += strlen(escape);
      }
      else
	  newbuf[newlen++] = c;
  }
  xf->xf_len = newlen;
  xf->xf_maxbuf = newmax;
  free(xf->xf_buf);
  xf->xf_buf = newbuf;
  return 0;
}
