/*
 *  CVS Version: $Id: xml.l,v 1.11 2013/08/15 11:31:20 olof Exp $
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

 * XML parser
 */

%{

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* clicon */
#include "xmlgen_xml.h"
#include "xml.tab.h"  
#include "xmlgen_parse.h"  

static int linenum;

/* Redefine main lex function so that you can send arguments to it */
#define YY_DECL int xmllex(void *ya)

/* Dont use input function (use user-buffer) */
#define YY_NO_INPUT

#undef xmlwrap
int xmlwrap(void)
{
  return 1;
}

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))



%}

%s BODY
%s COMMENT
%s STRING
%s TEXTDECL
%s TSTRING
%s QSTRING

%%
<INITIAL>[0-9A-Za-z_\-]+    { xmllval.string = strdup(yytext);
                            return NAME; 
                          }
<INITIAL>[ \t]+               ;
<INITIAL>\n                   linenum++;
<INITIAL>\:                  { return *xmltext; };
<INITIAL>"<?xml"             { BEGIN(TEXTDECL); return BEGINTEXTDECL;}
<INITIAL>"<!--"              { BEGIN(COMMENT); return BEGINCOMMENT; }
<INITIAL>"/>"                { BEGIN(BODY); return ENDSLASH; }
<INITIAL>"</"                { return BEGINSLASH; }
<INITIAL>\<                  { return *xmltext; }
<INITIAL>\>                  { BEGIN(BODY); return *xmltext; }
<INITIAL>[/=]                { return *xmltext; }
<INITIAL>\"                  { BEGIN(STRING); return *xmltext; }
<INITIAL>.                   { xmllval.string = yytext; return CHAR; /*XXX:optimize*/ }

<BODY>\n                  { xmllval.string = yytext;linenum++; return (CHAR);}
<BODY>"<!--"              { BEGIN(COMMENT); return BEGINCOMMENT; }
<BODY>"</"                { BEGIN(INITIAL); return BEGINSLASH; }
<BODY>\<                  { BEGIN(INITIAL); return *xmltext; }
<BODY>.                   { xmllval.string = yytext; return CHAR; /*XXX:optimize*/}

<COMMENT>"-->"               { BEGIN(INITIAL); return ENDCOMMENT; }
<COMMENT>\n                   linenum++;
<COMMENT>.                    ;
<TEXTDECL>encoding            { return ENCODING; }
<TEXTDECL>version             { return VERSION; }
<TEXTDECL>"="                 { return *xmltext; }
<TEXTDECL>"?>"                { BEGIN(INITIAL);return ENDTEXTDECL;}
<TEXTDECL>\"                  { BEGIN(TSTRING); return *xmltext; }
<TEXTDECL>\'                  { BEGIN(QSTRING); return *xmltext; }
<STRING>[^\"]+          { xmllval.string = xml_decode_attr_value(yytext);
                                 return CHAR; }
<STRING>\"                    { BEGIN(INITIAL); return *xmltext; }
<TSTRING>1\.[0-9]+       { xmllval.string = strdup(yytext);
                                 return CHAR; }
<TSTRING>[^\"]+          { xmllval.string = strdup(yytext);
                                 return CHAR; }
<TSTRING>\"                    { BEGIN(TEXTDECL); return *xmltext; }
<QSTRING>1\.[0-9]+       { xmllval.string = strdup(yytext);
                                 return CHAR; }
<QSTRING>[^\']+          { xmllval.string = strdup(yytext);
                                 return CHAR; }
<QSTRING>\'                    { BEGIN(TEXTDECL); return *xmltext; }

%%

int 
xmll_linenum(void)
{
  return linenum;
}

/*
 * xmll_init
 * Initialize scanner.
 * NOTE: string must be allocated all through the scanning!
 */
int
xmll_init(struct xml_parse_yacc_arg *ya)
{
  BEGIN(INITIAL);
  ya->ya_lexbuf = yy_scan_string (ya->ya_parse_string);
#if 1 /* XXX: just to use unput to avoid warning  */
  if (0)
    yyunput(0, ""); 
#endif
  return 0;
}

int
xmll_exit(struct xml_parse_yacc_arg *ya)
{
   yy_delete_buffer(ya->ya_lexbuf);
#if defined(YY_FLEX_SUBMINOR_VERSION) && YY_FLEX_SUBMINOR_VERSION >= 9
   xmllex_destroy();  /* modern */
#else
    yy_init = 1;      /* This does not quite free all buffers */
#endif

  return 0;
}
