/*
 *  CVS Version: $Id: xml.y,v 1.10 2013/08/01 09:15:47 olof Exp $
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
%union {
  char *string;
}

%start topxml

%token <string> NAME CHAR 
%token VER ENC
%token BSLASH ESLASH 
%token BTEXT ETEXT
%token BCOMMENT ECOMMENT 


%type <string> val aid

%lex-param     {void *_ya} /* Add this argument to parse() and lex() function */
%parse-param   {void *_ya}

%{

/* typecast macro */
#define _YA ((struct xml_parse_yacc_arg *)_ya)

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

/* clicon */
#include "clicon_err.h"
#include "clicon_log.h"
#include "clicon_buf.h"
#include "xmlgen_xml.h"
#include "clicon_xml_parse.h"

void 
clicon_xml_parseerror(void *_ya, char *s) 
{ 
  clicon_err(OE_XML, 0, "line %d: %s: at or before: %s", 
	      _YA->ya_linenum, s, clicon_xml_parsetext); 
  return;
}


static int
xml_attr_new(struct xml_parse_yacc_arg *ya, 
	     struct xml_node *xn, 
	     char *name, 
	     char *val)
{
    struct xml_node *xa; 

    if ((xa = xml_new(name, xn)) == NULL)
	return -1;
    xa->xn_type = XML_ATTRIBUTE;
    if ((xa->xn_value = strdup(val)) == NULL){
	clicon_err(OE_XML, errno, "strdup");
	return -1;
    }
    return 0;
}

/*
 * append string to existinbg body
 * assume only one character? (it could be escaped: &< )
 */
static struct xml_node *
xml_body_append(struct xml_parse_yacc_arg *ya, 
		struct xml_node *xn, 
		struct xml_node *xp, 
		char *str)
{
    int   sz;
    int   len;
    char  s0;

    s0 = str[0];
    if (xn != NULL){
	sz = strlen(xn->xn_value);
	if (s0 == ' ' || s0 == '\n' || s0 == '\t'){
	    str[0] = ' ';
	    if (xn->xn_value[sz-1] == ' ')
		return NULL;
	}
    }
    else{
	if (s0 == ' ' || s0 == '\n' || s0 == '\t')
	    return NULL;
	if ((xn = xml_new("body", xp)) == NULL)
	    return NULL;
	xn->xn_type = XML_BODY;
	sz = 0;
    }
    len = sz + strlen(str);
    if ((xn->xn_value = realloc(xn->xn_value, len+1)) == NULL){
	clicon_err(OE_XML, errno, "realloc");
	return NULL;
    }
    strncpy(xn->xn_value + sz, str, len-sz+1);
    return xn;
}


static int
xml_parse_version(struct xml_parse_yacc_arg *ya, char *ver)
{
    if(strcmp(ver, "1.0")){
	clicon_err(OE_XML, errno, "Wrong XML version %s expected 1.0\n", ver);
	free(ver);
	return -1;
    }
    free(ver);
    return 0;
}


static int
xml_parse_id(struct xml_parse_yacc_arg *ya, char *name, char *namespace)
{
    if ((ya->ya_xelement=xml_new(name, ya->ya_xparent)) == NULL) {
	if (namespace)
	    free(namespace);
	free(name);
	return -1;
    }
    ya->ya_xelement->xn_namespace = namespace;
    free(name);
    return 0;
}

static int
xml_parse_endslash_pre(struct xml_parse_yacc_arg *ya)
{
    ya->ya_xparent = ya->ya_xelement;
    ya->ya_xelement = NULL;
    return 0;
}

static int
xml_parse_endslash_mid(struct xml_parse_yacc_arg *ya)
{
    if (ya->ya_xelement != NULL)
	ya->ya_xelement = ya->ya_xelement->xn_parent;
    else
	ya->ya_xelement = ya->ya_xparent;
    ya->ya_xparent = ya->ya_xelement->xn_parent;
    return 0;
}

static int
xml_parse_endslash_post(struct xml_parse_yacc_arg *ya)
{
    ya->ya_xelement = NULL;
    return 0;
}

static int
xml_parse_bslash1(struct xml_parse_yacc_arg *ya, char *name)
{
    int retval = -1;

    if (strcmp(ya->ya_xelement->xn_name, name)){
	fprintf(stderr, "Sanity check failed: %s vs %s\n", 
		ya->ya_xelement->xn_name, name);
	goto done;
    }
    if (ya->ya_xelement->xn_namespace!=NULL){
	fprintf(stderr, "Sanity check failed: %s:%s vs %s\n", 
		ya->ya_xelement->xn_namespace,ya->ya_xelement->xn_name, name);
	goto done;
    }
    retval = 0;
  done:
    free(name);
    return retval;
}

static int
xml_parse_bslash2(struct xml_parse_yacc_arg *ya, char *namespace, char *name)
{
    int retval = -1;

    if (strcmp(ya->ya_xelement->xn_name, name)){
	fprintf(stderr, "Sanity check failed: %s:%s vs %s:%s\n", 
		ya->ya_xelement->xn_namespace, 
		ya->ya_xelement->xn_name, 
		namespace, 
		name);
	goto done;
    }
    if (ya->ya_xelement->xn_namespace==NULL ||
	strcmp(ya->ya_xelement->xn_namespace, namespace)){
	fprintf(stderr, "Sanity check failed: %s:%s vs %s:%s\n", 
		ya->ya_xelement->xn_namespace, 
		ya->ya_xelement->xn_name, 
		namespace, 
		name);
	goto done;
    }
    retval = 0;
  done:
    free(name);
    free(namespace);
    return retval;
}

static int
xml_parse_content(struct xml_parse_yacc_arg *ya, char *str)
{
    int retval = -1;

    if ((ya->ya_xelement=xml_body_append(ya, 
					 ya->ya_xelement, 
					 ya->ya_xparent, 
					 str)) == NULL) 
	goto done;
    retval = 0;
  done:
//    free(str);
    return retval;
}

static char*
xml_parse_ida(struct xml_parse_yacc_arg *ya, char *namespace, char *name)
{
    char *str;
    int len = strlen(namespace)+strlen(name)+2;

    if ((str=malloc(len)) == NULL)
	return NULL;
    snprintf(str, len, "%s:%s", namespace, name);
    free(namespace);
    free(name);
    return str;
}

static int
xml_parse_attr(struct xml_parse_yacc_arg *ya, char *id, char *val)
{
    int retval = -1;

    if (xml_attr_new(ya, ya->ya_xelement, id, val) < 0) 
	goto done;
    retval = 0;
  done:
    free(id); 
    free(val);
    return retval;
}

%} 
 
%%

topxml      : list
                   { clicon_debug(2, "ACCEPT\n"); YYACCEPT; }
            | dcl list
                   { clicon_debug(2, "ACCEPT\n"); YYACCEPT; }
            ;

dcl         : BTEXT info encode ETEXT
            ;

info        : VER '=' '\"' CHAR '\"' 
                 { if (xml_parse_version(_YA, $4) <0) YYABORT; }
            | VER '=' '\'' CHAR '\'' 
         	 { if (xml_parse_version(_YA, $4) <0) YYABORT; }
            |
            ;

encode      : ENC '=' '\"' CHAR '\"' {free($4);}
            | ENC '=' '\'' CHAR '\'' {free($4);}
            ;

element     : '<' id  attrs element1
	      ;

id          : NAME            { if (xml_parse_id(_YA, $1, NULL) < 0) YYABORT; }
            | NAME ':' NAME   { if (xml_parse_id(_YA, $3, $1) < 0) YYABORT; }
            ;

element1    :  ESLASH {_YA->ya_xelement = NULL;} 
            | '>'              { xml_parse_endslash_pre(_YA); }
              list             { xml_parse_endslash_mid(_YA); }
              etg             { xml_parse_endslash_post(_YA); }
            ;

etg         : BSLASH NAME '>'          
                       { if (xml_parse_bslash1(_YA, $2) < 0) YYABORT; }
            | BSLASH NAME ':' NAME '>' 
                       { if (xml_parse_bslash2(_YA, $2, $4) < 0) YYABORT; }
            ;

list        : list content 
            | 
            ;

content     : element 
            | comment 
            | CHAR   { if (xml_parse_content(_YA, $1) < 0) YYABORT; }

            ;

comment     : BCOMMENT ECOMMENT
            ;


attrs       : attrs att
            |
            ;


aid         : NAME   {$$ = $1;}
            | NAME ':' NAME  
                     { if (($$ = xml_parse_ida(_YA, $1, $3)) == NULL) YYABORT; }
            ;

att         : aid '=' val { if (xml_parse_attr(_YA, $1, $3) < 0) YYABORT; }
            ;

val         : '\"' CHAR '\"'   { $$=$2; /* $2 must be consumed */}
            | '\"'  '\"'       { $$=strdup(""); /* $2 must be consumed */}
            ;

%%

