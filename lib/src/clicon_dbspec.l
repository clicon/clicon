/*
 *  CVS Version: $Id: clicon_dbspec.l,v 1.3 2013/09/13 15:05:14 olof Exp $
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

 * Database specification parser cli syntax
 * (Cloned from cligen parser)
 */

%{


#include "clicon_config.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <netinet/in.h>

#include "clicon_dbspec.tab.h" /* generated */

#include <cligen/cligen.h>

#include "clicon_queue.h"
#include "clicon_hash.h"
#include "clicon_handle.h"
#include "clicon_spec.h"
#include "clicon_dbspec_parsetree.h"
#include "clicon_dbspec.h"

/* Redefine main lex function so that you can send arguments to it */
#define YY_DECL int clicon_dbspeclex(void *_ya)

/* Dont use input function (use user-buffer) */
#define YY_NO_INPUT

/* typecast macro */
#define _YA ((struct clicon_dbspec_yacc_arg *)_ya)

#define     MAXBUF  4*4*64*1024

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

#undef clicon_dbspecwrap
int 
clicon_dbspecwrap(void)
{
  return 1;
}

/* like strdup but strip \:s */
static char *
stripdup(char *s0)
{
  char *s1;
  char *s;

  if ((s1 = strdup(s0)) == NULL){
    fprintf(stderr, "%s: strdup: %s\n", __FUNCTION__, strerror(errno));
    return NULL;
  }
  while ((s = index(s1, '\\')) != NULL)
    memmove(s, s+1, strlen(s)); 
  return s1;
}

/*
  See block_chart for lex states.
*/

%}

%s OPTION
%s HELP
%s COMMENT
%s NAMEORTYPE
%s VARIABLE
%s ESCAPE
%s STRING

%%
<INITIAL>[ \t]            
<INITIAL>,                { BEGIN(OPTION); return *yytext; }
<INITIAL>#                { _YA->ya_lex_state =INITIAL; BEGIN(COMMENT); }
<INITIAL>\n               { _YA->ya_linenum++; }
<INITIAL>=                { BEGIN(OPTION); return *yytext; }
<INITIAL>;                { return *yytext; }
<INITIAL><<EOF>>          { return MY_EOF; }
<INITIAL>\<               { BEGIN(NAMEORTYPE); return *yytext; }
<INITIAL>\(               { return *yytext; }
<INITIAL>\)               { return *yytext; }
<INITIAL>\|               { return *yytext; }
<INITIAL>\(\"             { BEGIN(HELP); return PDQ; /* parenth double-q*/  }
<INITIAL>\[               { return *yytext; }
<INITIAL>\]               { return *yytext; }
<INITIAL>\{               { return *yytext; }
<INITIAL>\}               { return *yytext; }
<INITIAL>\@               { return *yytext; }
<INITIAL>([^@ \t,#\n=;\\<\(\)\[\]\|\{\}]|\\.)+  { 
                            clicon_dbspeclval.string = stripdup(yytext);
                            return NAME; }
<INITIAL>.                { return -1; }

<HELP>\n                  { _YA->ya_linenum++; }
<HELP><<EOF>>             { return MY_EOF; }
<HELP>\"\)                { BEGIN(INITIAL); return DQP; /* double-quote parenthes */}
<HELP>\\                  { _YA->ya_lex_state = HELP; BEGIN(ESCAPE); }
<HELP>.                   { clicon_dbspeclval.string = strdup(yytext);
                            return CHAR;}

<NAMEORTYPE>\>            { BEGIN(INITIAL); return *yytext; }
<NAMEORTYPE>\n            { _YA->ya_linenum++; }
<NAMEORTYPE>\:            { return *yytext; }
<NAMEORTYPE>\[              { return *yytext; }
<NAMEORTYPE>\]              { return *yytext; }
<NAMEORTYPE>[ \t]+        { BEGIN(VARIABLE); return ' '; }
<NAMEORTYPE>([^ \t>:\]\[]|\\.)+  { 
                            clicon_dbspeclval.string = strdup(yytext);
                            return NAME; }

<VARIABLE>[ \t]+          { return ' '; }
<VARIABLE>\n              { _YA->ya_linenum++; }
<VARIABLE>\>              { BEGIN(INITIAL); return *yytext; }
<VARIABLE>\:              { return *yytext; }
<VARIABLE>\-              { return *yytext; }
<VARIABLE>\|              { return *yytext; }
<VARIABLE>\(              { return *yytext; }
<VARIABLE>\)              { return *yytext; }
<VARIABLE>\[              { return *yytext; }
<VARIABLE>\]              { return *yytext; }
<VARIABLE>\"              { _YA->ya_lex_string_state =VARIABLE;BEGIN(STRING); return DQ; }
<VARIABLE>default         { return V_DEFAULT; }
<VARIABLE>type            { return V_TYPE; }
<VARIABLE>range           { return V_RANGE; }
<VARIABLE>choice          { return V_CHOICE; }
<VARIABLE>mandatory       { return V_MANDATORY; }
<VARIABLE>optional        { return V_OPTIONAL; }
<VARIABLE>keyword         { return V_KEYWORD; }
<VARIABLE>regexp          { return V_REGEXP; }
<VARIABLE>[-+]?[0-9]+          { clicon_dbspeclval.string = strdup(yytext); return NUMBER;}
<VARIABLE>([^ \t\n>:\-\|\"\(\)\[\]]|\\.)+ { 
                                 clicon_dbspeclval.string = strdup(yytext);
                                 return NAME; }

<OPTION>[ \t]+            
<OPTION>,                { return *yytext; }
<OPTION>#                 { _YA->ya_lex_state =INITIAL; BEGIN(COMMENT); }
<OPTION>\n                { _YA->ya_linenum++; }
<OPTION>=                 { return *yytext; }
<OPTION>;                { BEGIN(INITIAL); return *yytext;}
<OPTION><<EOF>>           { return MY_EOF; }
<OPTION>\(                { return *yytext; }
<OPTION>\)                { return *yytext; }
<OPTION>\{                { BEGIN(INITIAL); return *yytext;}
<OPTION>\"                { _YA->ya_lex_string_state =INITIAL;BEGIN(STRING); return DQ; }
<OPTION>([^ \t,#\n=;\(\)\{\}\"]|\\.)+   { 
                                 clicon_dbspeclval.string = strdup(yytext);
                                 return NAME; }
<OPTION>.                 { return -1;}

<STRING>\n                { _YA->ya_linenum++; }
<STRING>\\                { _YA->ya_lex_state = STRING; BEGIN(ESCAPE); }
<STRING>\"                { BEGIN(_YA->ya_lex_string_state); return DQ; }
<STRING>.                 { clicon_dbspeclval.string = strdup(yytext);
                            return CHAR;}

<ESCAPE>.                 { BEGIN(_YA->ya_lex_state); 
                             clicon_dbspeclval.string = strdup(yytext); 
                             return CHAR; }

<COMMENT>\n               { _YA->ya_linenum++; BEGIN(_YA->ya_lex_state);}
<COMMENT><<EOF>>          { return MY_EOF; }
<COMMENT>.               


%%

/*
 * dbspec_parse_init
 * Initialize scanner.
 */
int
dbspec_scan_init(struct clicon_dbspec_yacc_arg *ya)
{
  BEGIN(INITIAL);
  ya->ya_lexbuf = yy_scan_string (ya->ya_parse_string);
#if 1 /* XXX: just to use unput to avoid warning  */
  if (0)
    yyunput(0, ""); 
#endif

  return 0;
}

/*
 * dbspec_parse_exit
 * free buffers
 * Even within Flex version 2.5 (this is assumed), freeing buffers is different.
 */
int
dbspec_scan_exit(struct clicon_dbspec_yacc_arg *ya)
{
    yy_delete_buffer(ya->ya_lexbuf);
#if defined(YY_FLEX_SUBMINOR_VERSION) && YY_FLEX_SUBMINOR_VERSION >= 9
    clicon_dbspeclex_destroy();  /* modern */
#else
    yy_init = 1;      /* This does not quite free all buffers */
#endif
    return 0;
}


