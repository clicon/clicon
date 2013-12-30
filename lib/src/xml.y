/*
 *  CVS Version: $Id: xml.y,v 1.10 2013/08/01 09:15:47 olof Exp $
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
%union {
  int intval;
  char *string;
}

%left '-' '+'
%left '*' '/'
%left '>' '<' '='
%left AND_OP OR_OP
%left NEG
%left '!' 
  
%start xml

%token <intval> NUMBER
%token <string> NAME CHAR VERSIONNUM
%token BEGINCOMMENT ENDCOMMENT BEGINSLASH ENDSLASH 
%token BEGINTEXTDECL ENDTEXTDECL
%token VERSION ENCODING

%type <string> AttValue IdA

%lex-param     {void *_ya} /* Add this argument to parse() and lex() function */
%parse-param   {void *_ya}

%{

/* typecast macro */
#define _YA ((struct xml_parse_yacc_arg *)_ya)

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

/* clicon */
#include "clicon_err.h"
#include "xmlgen_xf.h"
#include "xmlgen_xml.h"
#include "xmlgen_xf.h"
#include "xmlgen_parse.h"

static struct xml_node *xnc; /* current xml element */
static struct xml_node *xnp; /* current xml parent */

static char helpstr[64]; /* For use in error */
static int debug = 0;

int 
xmly_debug(int value)
{
    int oldvalue = debug;

    debug = value;
    return oldvalue;
}

void 
xmlerror(void *ya, char *s) 
{ 
  extern  char    *xmltext;
  int linenum;

  linenum = xmll_linenum();
  clicon_err(OE_XML, 0, "%s line %d: %s: at or before: %s", 
	  helpstr, linenum, s, xmltext); 
  return;
}

int 
xmly_init(struct xml_node *xn_parent, char *helpstr0)
{
    strncpy(helpstr, helpstr0, sizeof(helpstr)-1);
    xnp = xn_parent;
    xnc = NULL;
    return 0;
}

/*
 * create or append to existing body
 * assume only one character? (it could be escaped: &< )
 * XXX: when body done, call xml_decode_attr_value() on it
 */
static struct xml_node *
append_body(struct xml_node *xn, struct xml_node *xn_parent, char *str)
{
    int len0, len;

    if (debug)
	fprintf(stderr, "%s: '%s'\n", __FUNCTION__, str);

    if (xn == NULL){
	/* if start w space, CR, TAB, ignore */
	if (str[0] == ' ' || str[0] == '\n' || str[0] == '\t')
	    return NULL;
	assert(xn_parent);
	if ((xn = xml_new("body", xn_parent)) == NULL)
	    return NULL;
	xn->xn_type = XML_BODY;
	len0 = 0;
    }
    else{ /* ignore more han one space, rm cr, tab */
	len0 = strlen(xn->xn_value);
	if (str[0] == ' ' || str[0] == '\n' || str[0] == '\t'){
	    str[0] = ' ';
	    if (xn->xn_value[len0-1] == ' ')
		return NULL;
	}
    }
    len = len0 + strlen(str);
    if ((xn->xn_value = realloc(xn->xn_value, len+1)) == NULL){
	clicon_err(OE_XML, errno, "realloc");
	return NULL;
    }
    strncpy(xn->xn_value + len0, str, len-len0+1);
    return xn;
}

static int
create_attribute(struct xml_node *xn, char *name, char *attrval)
{
    struct xml_node *xa; 

    if (debug)
	fprintf(stderr, "%s: %s\n", __FUNCTION__, name);
    assert(xn);
    if ((xa = xml_new(name, xn)) == NULL)
	return -1;
    xa->xn_type = XML_ATTRIBUTE;
    if ((xa->xn_value = strdup(attrval)) == NULL){
	clicon_err(OE_XML, errno, "strdup");
	return -1;
    }
    return 0;
}

%} 
 
%%

xml        : contentlist
             { if (debug) fprintf(stderr, "ACCEPT\n"); 
               YYACCEPT; }
            | XMLDecl contentlist
            { if (debug) fprintf(stderr, "ACCEPT\n"); 
               YYACCEPT; }
            ;

XMLDecl   : BEGINTEXTDECL VersionInfo EncodingDecl ENDTEXTDECL
           ;

VersionInfo : VERSION '=' '\"' CHAR '\"' 
            {
		if(strcmp($4, "1.0")){
		    clicon_err(OE_XML, errno, "Wrong XML version %s expected 1.0\n", $4);
		    free($4);
		     YYABORT;
		}
		 free($4);
	     }
           | VERSION '=' '\'' CHAR '\'' 
            {
		if(strcmp($4, "1.0")){
		    clicon_err(OE_XML, errno, "Wrong XML version %s expected 1.0\n", $4);
		    free($4);
		     YYABORT;
		}
		 free($4);
	     }

            |
            ;

EncodingDecl : ENCODING '=' '\"' CHAR '\"' {free($4);}
             | ENCODING '=' '\'' CHAR '\'' {free($4);}
            ;

element     : '<' 
             Id  
              Attributes
	      element1
	      ;
Id          : NAME {if ((xnc=xml_new($1, xnp)) == NULL) {free($1);YYABORT;}free($1);}
            | NAME ':' NAME 
	    { /* $1 is namespace*/
		if ((xnc=xml_new($3, xnp)) == NULL) {
		    free($1),free($3);
		    YYABORT;
		}
		xnc->xn_namespace = $1;
		free($3);
	    }
            ;
element1    :  ENDSLASH {xnc = NULL;} 
            | '>'
              {xnp=xnc;xnc = NULL;} 
              contentlist 
              {xnc = xnc?xnc->xn_parent:xnp;
               xnp=xnc->xn_parent;}  
              ETag {xnc = NULL;}
             ;

ETag         : BEGINSLASH NAME '>' 
                  {
		      if (strcmp(xnc->xn_name, $2)){
			  fprintf(stderr, "Sanity check failed: %s vs %s\n", 
				  xnc->xn_name, $2);
			  free($2);
			  YYABORT;
		      }
		      if (xnc->xn_namespace!=NULL){
    			  fprintf(stderr, "Sanity check failed: %s:%s vs %s\n", 
				  xnc->xn_namespace,xnc->xn_name, $2);
			  free($2);
			  YYABORT;
		      }
		      free($2);
		  }
             | BEGINSLASH NAME ':' NAME '>' 
	     {
		 if (strcmp(xnc->xn_name, $4)){
		     fprintf(stderr, "Sanity check failed: %s:%s vs %s:%s\n", 
			     xnc->xn_namespace, xnc->xn_name, $2, $4);
		     free($2); free($4); 	
		     YYABORT;
		 }
		 if (xnc->xn_namespace==NULL ||strcmp(xnc->xn_namespace, $2)){
		     fprintf(stderr, "Sanity check failed: %s:%s vs %s:%s\n", 
			     xnc->xn_namespace, xnc->xn_name, $2, $4);
		     free($2); free($4); 	
		     YYABORT;
		 }
		 free($2); free($4); 	
	     }
             ;

contentlist : contentlist content 
            | 
            ;

content     : element 
            | comment 
            | CHAR {    
		if ((xnc=append_body(xnc, xnp, $1)) == NULL) {
		    /*free($1); YYABORT;*/
		}
//XXX		free($1);
	      }
            ;

comment     : BEGINCOMMENT ENDCOMMENT
            ;


Attributes   :  Attributes Attribute
             |
             ;


IdA          : NAME {$$=$1;}
            | NAME ':' NAME 
	    { /* $1 is namespace*/
		int len = strlen($1)+strlen($3)+2;
		if (($$=malloc(len)) == NULL)
		    YYABORT;
		snprintf($$, len, "%s:%s", $1, $3);
		free($1);
		free($3);
	    }
            ;
Attribute    : IdA '=' AttValue {if (create_attribute(xnc, $1, $3) < 0) {free($1); free($3);YYABORT;} free($1); free($3);}
             ;

AttValue     : '\"' CHAR '\"' { $$=$2; /* $2 must be consumed */}
             | '\"'  '\"' { $$=strdup(""); /* $2 must be consumed */}
             ;

%%

