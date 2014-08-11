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
 */

/*
 * CLICON dynamic buffers 
 * Usage:
 * cbuf *xf;
 * xf = cbuf_new();
 * write(f, cbuf_get(xf), cbuf_len(xf));
 * cbuf_free(xf);
 */

/*
 * Constants
 */
#define CBUFLEN_START 8*1024   /* Start length */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clicon_buf.h"

/*! CLICON buffer hidden from external view 
 */
struct cbuf {
    char  *cb_buffer;     
    size_t cb_buflen;  
    size_t cb_strlen;  
};

/*! Allocate clicon buffer. The handle returned can be used in  successive sprintf calls
 * which dynamically print a string.
 * The handle should be freed by cbuf_free()
 * Return the allocated object handle on success.
 * Returns NULL on error.
 */
cbuf *
cbuf_new(void)
{
    cbuf *cb;

    if ((cb = (cbuf*)malloc(sizeof(*cb))) == NULL)
	return NULL;
    memset(cb, 0, sizeof(*cb));
    if ((cb->cb_buffer = malloc(CBUFLEN_START)) == NULL)
	return NULL;
    cb->cb_buflen = CBUFLEN_START;
    memset(cb->cb_buffer, 0, cb->cb_buflen);
    cb->cb_strlen = 0;
    return cb;
}

/*! Free clicon buffer previously allocated with cbuf_new
 */
void
cbuf_free(cbuf *cb)
{
    if (cb->cb_buffer)
	free(cb->cb_buffer);
    free(cb);
}

/*! Return actual byte buffer of clicon buffer
 */
char*
cbuf_get(cbuf *cb)
{
    return cb->cb_buffer;
}

/*! Return length of clicon buffer
 */
int
cbuf_len(cbuf *cb)
{
    return cb->cb_strlen;
}

/*! Reset a CLICON buffer. That is, restart it from scratch.
 */
void
cbuf_reset(cbuf *cb)
{
    cb->cb_strlen    = 0; 
    cb->cb_buffer[0] = '\0'; 
}

/*! Create clicon buf by printf like semantics
 * 
 * @param [in]  cb      CLICON buffer allocated by cbuf_new(), may be reallocated.
 * @param [in]  format  arguments uses printf syntax.
 * @retval      See printf
 */
int
cprintf(cbuf *cb, const char *format, ...)
{
    va_list ap;
    int diff;
    int retval;

    va_start(ap, format);
  again:
    if (cb == NULL)
	return 0;
    retval = vsnprintf(cb->cb_buffer+cb->cb_strlen, 
		       cb->cb_buflen-cb->cb_strlen, 
		       format, ap);
    if (retval < 0)
	return -1;
    diff = cb->cb_buflen - (cb->cb_strlen + retval + 1);
    if (diff <= 0){
	cb->cb_buflen *= 2;
	if ((cb->cb_buffer = realloc(cb->cb_buffer, cb->cb_buflen)) == NULL)
	    return -1;
	va_end(ap);
	va_start(ap, format);
	goto again;
    }
    cb->cb_strlen += retval;

    va_end(ap);
    return retval;
}




