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
 * cbuf *b;
 * b = cbuf_new();
 * cprintf(b, "%d %s", 43, "go");
 * write(f, cbuf_get(b), cbuf_len(b));
 * cbuf_free(b);
 */

#ifndef _CLICON_BUF_H
#define _CLICON_BUF_H

#include <stdarg.h> 

/*
 * Types
 */
typedef struct cbuf cbuf; /* clicon buffer type is fully defined in c-file */

/*
 * Prototypes
 */
cbuf *cbuf_new(void);
void  cbuf_free(cbuf *cb);
char *cbuf_get(cbuf *cb);
int   cbuf_len(cbuf *cb);
int   cprintf(cbuf *cb, const char *format, ...);
void  cbuf_reset(cbuf *cb);

#endif /* _CLICON_BUF_H */
