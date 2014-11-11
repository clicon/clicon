/*
 *
  Copyright (C) 2009-2014 Olof Hagsand and Benny Holmgren
  Olof Hagsand
 *
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

#ifndef _CLICON_STRING_H_
#define _CLICON_STRING_H_

/*! A strdup version of strdup that aligns on 4 bytes. To avoid warning from valgrind */
static inline char * strdup4(char *str) 
{
    char *dup;
    int len;
    len = ((strlen(str)+1)/4)*4 + 4;
    if ((dup = malloc(len)) == NULL)
	return NULL;
    strncpy(dup, str, len);
    return dup;
}

/*
 * Prototypes
 */ 
char **clicon_sepsplit (char *string, char *delim, int *nvec, const char *label);
char **clicon_strsplit (char *string, char *delim, int *nvec, const char *label);
char *clicon_strjoin (int argc, char **argv, char *delim, const char *label);
char *clicon_strtrim(char *str, const char *label);
int clicon_sep(char *s, const char sep[2], const char *label, char**a0, char **b0);
#ifndef HAVE_STRNDUP
char *clicon_strndup (const char *, size_t);
#endif /* ! HAVE_STRNDUP */
int clicon_strmatch(const char *str, const char *regexp, char **match);


#endif  /* _CLICON_STRING_H_ */
