/*
 *  CVS Version: $Id: xmlgen_parse.h,v 1.4 2013/08/01 09:15:46 olof Exp $
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

 * XML parser
 */
#ifndef _XMLGEN_PARSE_H_
#define _XMLGEN_PARSE_H_

/*
 * Types
 */
struct xml_parse_yacc_arg{
    char                 *ya_parse_string; /* original (copy of) parse string */
    void                 *ya_lexbuf;       /* internal parse buffer from lex */
};

/*
 * Prototypes
 */
int xmll_init(struct xml_parse_yacc_arg *ya);
int xmll_exit(struct xml_parse_yacc_arg *ya);
int xmly_init(struct xml_node *xn_parent, char *helpstr);

int xmll_linenum(void);
int xmllex(void *);
int xmlparse(void *);
void xmlgen_xmlerror(char*);

#endif	/* _XMLGEN_PARSE_H_ */
