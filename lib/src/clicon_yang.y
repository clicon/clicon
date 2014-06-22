/*
 *  CVS Version: $Id: clicon_dbspec.y,v 1.4 2013/09/18 19:14:53 olof Exp $
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

 * Cloned from cligen parser. But made the following changes:


   The argument is a string, as defined in Section 6.1.2.
 */


%start file

%union {
  int intval;
  char *string;
}

%token MY_EOF 
%token DQ           /* Double quote: " */
%token K_UNKNOWN    /* template for error */
%token <string> NAME /* in variables: <NAME type:NAME> */
%token <string> CHAR

%type <string> ustring qstring string id_arg_str

/* rfc 6020 keywords */
%token K_ANYXML
%token K_ARGUMENT
%token K_AUGMENT
%token K_BASE
%token K_BELONGS_TO
%token K_BIT
%token K_CASE
%token K_CHOICE
%token K_CONFIG
%token K_CONTACT
%token K_CONTAINER
%token K_DEFAULT
%token K_DESCRIPTION
%token K_DEVIATE
%token K_DEVIATION
%token K_ENUM
%token K_ERROR_APP_TAG
%token K_ERROR_MESSAGE
%token K_EXTENSION
%token K_FEATURE
%token K_FRACTION_DIGITS
%token K_GROUPING
%token K_IDENTITY
%token K_IF_FEATURE
%token K_IMPORT
%token K_INCLUDE
%token K_INPUT
%token K_KEY
%token K_LEAF
%token K_LEAF_LIST
%token K_LENGTH
%token K_LIST
%token K_MANDATORY
%token K_MAX_ELEMENTS
%token K_MIN_ELEMENTS
%token K_MODULE
%token K_MUST
%token K_NAMESPACE
%token K_NOTIFICATION
%token K_ORDERED_BY
%token K_ORGANIZATION
%token K_OUTPUT
%token K_PATH
%token K_PATTERN
%token K_POSITION
%token K_PREFIX
%token K_PRESENCE
%token K_RANGE
%token K_REFERENCE
%token K_REFINE
%token K_REQUIRE_INSTANCE
%token K_REVISION
%token K_REVISION_DATE
%token K_RPC
%token K_STATUS
%token K_SUBMODULE
%token K_TYPE
%token K_TYPEDEF
%token K_UNIQUE
%token K_UNITS
%token K_USES
%token K_VALUE
%token K_WHEN
%token K_YANG_VERSION
%token K_YIN_ELEMENT


%lex-param     {void *_ya} /* Add this argument to parse() and lex() function */
%parse-param   {void *_ya}

%{
/* Here starts user C-code */

/* typecast macro */
#define _YA ((struct clicon_yang_yacc_arg *)_ya)

/* add _ya to error paramaters */
#define YY_(msgid) msgid 

#include "clicon_config.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include <cligen/cligen.h>

#include "clicon_queue.h"
#include "clicon_hash.h"
#include "clicon_handle.h"
#include "clicon_spec.h"
#include "clicon_err.h"
#include "clicon_log.h"
#include "clicon_yang_parse.h"

extern int clicon_yangget_lineno  (void);

int 
clicon_yang_debug(int d)
{
    debug = d;
    return 0;
}

/* 
   clicon_yangerror
   also called from yacc generated code *
*/
void clicon_yangerror(void *_ya, char *s) 
{ 
  fprintf(stderr, "%s%s%d: Error: %s: at or before: '%s'\n", 
	  _YA->ya_name,
	   ":" ,
	  _YA->ya_linenum ,
	  s, 
	  clicon_yangtext); 
  return;
}

int
yang_parse_init(struct clicon_yang_yacc_arg *ya, cg_obj *co_top)
{
    return 0;
}


int
yang_parse_exit(struct clicon_yang_yacc_arg *ya)
{
    return 0;
}



%} 
 
%%

/*
   statement = keyword [argument] (";" / "{" *statement "}") 
   The argument is a string
   recursion: right is wrong
   Let subststmt rules contain an empty rule, but not stmt rules
*/

file          : module_stmt MY_EOF
                       { clicon_debug(2,"file->module-stmt"); YYACCEPT; } 
              | submodule_stmt MY_EOF
                       { clicon_debug(2,"file->submodule-stmt"); YYACCEPT; } 
              ;

unknown_stmt  : K_UNKNOWN      { clicon_debug(2,"unknown-stmt"); YYERROR; } 
              ;

/* module */
module_stmt   : K_MODULE id_arg_str '{' module_substmts '}' 
                      { clicon_debug(2,"module -> id-arg-str { module-stmts }");} 
              ;

module_substmts : module_substmts module_substmt 
                      { clicon_debug(2,"module-substmts -> module-substmts module-substm");} 
              | module_substmt 
                      { clicon_debug(2,"module-substmts ->");} 
              ;

module_substmt : module_header_stmts { clicon_debug(2,"module-substmt -> module-header-stmts");}
               | linkage_stmts       { clicon_debug(2,"module-substmt -> linake-stmts");} 
               | meta_stmts          { clicon_debug(2,"module-substmt -> meta-stmts");} 
               | revision_stmts      { clicon_debug(2,"module-substmt -> revision-stmts");} 
               | body_stmts          { clicon_debug(2,"module-substmt -> body-stmts");} 
               | unknown_stmt        { clicon_debug(2,"module-substmt -> unknown-stmt");} 
               |                     { clicon_debug(2,"module-substmt ->");} 
               ;

/* submodule */
submodule_stmt : K_SUBMODULE id_arg_str '{' submodule_substmts '}' 
                      { clicon_debug(2,"submodule -> id-arg-str { submodule-stmts }"); }
              ;

submodule_substmts : submodule_substmts submodule_substmt 
                       { clicon_debug(2,"submodule-stmts -> submodule-substmts submodule-substmt"); }
              | submodule_substmt       
                       { clicon_debug(2,"submodule-stmts -> submodule-substmt"); }
              ;

submodule_substmt : submodule_header_stmts 
                              { clicon_debug(2,"submodule-substmt -> submodule-header-stmts"); }
               | linkage_stmts  { clicon_debug(2,"submodule-substmt -> linake-stmts");} 
               | meta_stmts     { clicon_debug(2,"submodule-substmt -> meta-stmts");} 
               | revision_stmts { clicon_debug(2,"submodule-substmt -> revision-stmts");} 
               | body_stmts     { clicon_debug(2,"submodule-stmt -> body-stmts"); }
               | unknown_stmt   { clicon_debug(2,"submodule-substmt -> unknown-stmt");} 
               |                { clicon_debug(2,"submodule-substmt ->");} 
              ;

/* module-header */
module_header_stmts : module_header_stmts module_header_stmt
                  { clicon_debug(2,"module-header-stmts -> module-header-stmts module-header-stmt"); }
              | module_header_stmt   { clicon_debug(2,"module-header-stmts -> "); }
              ;

module_header_stmt : yang_version_stmt 
                               { clicon_debug(2,"module-header-stmt -> yang-version-stmt"); }
              | namespace_stmt { clicon_debug(2,"module-header-stmt -> namespace-stmt"); }
              | prefix_stmt    { clicon_debug(2,"module-header-stmt -> prefix-stmt"); }
              ;

/* submodule-header */
submodule_header_stmts : submodule_header_stmts submodule_header_stmt
                  { clicon_debug(2,"submodule-header-stmts -> submodule-header-stmts submodule-header-stmt"); }
              | submodule_header_stmt   
                  { clicon_debug(2,"submodule-header-stmts -> submodule-header-stmt"); }
              ;

submodule_header_stmt : yang_version_stmt 
                               { clicon_debug(2,"submodule-header-stmt -> yang-version-stmt"); }
              ;

/* linkage */
linkage_stmts : linkage_stmts linkage_stmt 
                       { clicon_debug(2,"linkage-stmts -> linkage-stmts linkage-stmt"); }
              | linkage_stmt
                       { clicon_debug(2,"linkage-stmts -> linkage-stmt"); }
              ;

linkage_stmt  : import_stmt  { clicon_debug(2,"linkage-stmt -> import-stmt"); }
              | include_stmt { clicon_debug(2,"linkage-stmt -> include-stmt"); }
              ;

/* revision */
revision_stmts : revision_stmts revision_stmt 
                       { clicon_debug(2,"revision-stmts -> revision-stmts revision-stmt"); }
              | revision_stmt
                       { clicon_debug(2,"revision-stmts -> "); }
              ;

revision_stmt : K_REVISION string ';'  /* XXX date-arg-str */
                     { clicon_debug(2,"revision-stmt -> date-arg-str ;"); }
              | K_REVISION string '{' revision_substmts '}'  /* XXX date-arg-str */
                     { clicon_debug(2,"revision-stmt -> date-arg-str { revision-substmts  }"); }
              ;

revision_substmts : revision_substmts revision_substmt 
                     { clicon_debug(2,"revision-substmts -> revision-substmts revision-substmt }"); }
              | revision_substmt
                     { clicon_debug(2,"revision-substmts -> }"); }
              ;

revision_substmt : description_stmt { clicon_debug(2,"revision-substmt -> description-stmt"); }
              | reference_stmt      { clicon_debug(2,"revision-substmt -> reference-stmt"); }
              | unknown_stmt        { clicon_debug(2,"revision-substmt -> unknown-stmt");} 
              |                     { clicon_debug(2,"revision-substmt -> "); }
              ;

/* meta */
meta_stmts    : meta_stmts meta_stmt { clicon_debug(2,"meta-stmts -> meta-stmts meta-stmt"); }
              | meta_stmt            { clicon_debug(2,"meta-stmts -> meta-stmt"); }
              ;

meta_stmt     : organization_stmt    { clicon_debug(2,"meta-stmt -> organization-stmt"); }
              | contact_stmt         { clicon_debug(2,"meta-stmt -> contact-stmt"); }
              | description_stmt     { clicon_debug(2,"meta-stmt -> description-stmt"); }
              | reference_stmt       { clicon_debug(2,"meta-stmt -> reference-stmt"); }
              ;

/* body */
body_stmts    : body_stmts body_stmt { clicon_debug(2,"body-stmts -> body-stmts body-stmt"); } 
              | body_stmt            { clicon_debug(2,"body-stmts -> body-stmt");}
              ;

body_stmt     : data_def_stmt        { clicon_debug(2,"body-stmt -> data-def-stmt");}
              ;

data_def_stmt : container_stmt       { clicon_debug(2,"data-def-stmt -> container-stmt");}
              | leaf_stmt            { clicon_debug(2,"data-def-stmt -> leaf-stmt");}
              | leaf_list_stmt       { clicon_debug(2,"data-def-stmt -> leaf-list-stmt");}
              | list_stmt            { clicon_debug(2,"data-def-stmt -> list-stmt");}
              ;

/* container */
container_stmt : K_CONTAINER id_arg_str ';'
                           { clicon_debug(2,"container-stmt -> CONTAINER id-arg-str ;");}
              | K_CONTAINER id_arg_str '{' container_substmts '}'
                           { clicon_debug(2,"container-stmt -> CONTAINER id-arg-str { container-substmts }");}
              ;

container_substmts : container_substmts container_substmt 
              | container_substmt 
              ;

container_substmt : data_def_stmt   { clicon_debug(2,"container-substmt -> data-def-stmt");} 
              | description_stmt    { clicon_debug(2,"container-substmt -> description-stmt");} 
              | reference_stmt      { clicon_debug(2,"container-substmt -> reference-stmt"); }
              | unknown_stmt        { clicon_debug(2,"container-substmt -> unknown-stmt");} 
              |                     { clicon_debug(2,"container-substmt ->");} 
              ;

/* leaf */
leaf_stmt     : K_LEAF id_arg_str ';'
              | K_LEAF id_arg_str '{' leaf_substmts '}' 
              ;

leaf_substmts : leaf_substmts leaf_substmt
              | leaf_substmt
              ;

leaf_substmt  : type_stmt            { clicon_debug(2,"leaf-substmt -> type-stmt"); }
              | description_stmt     { clicon_debug(2,"leaf-substmt -> description-stmt"); }
              | reference_stmt       { clicon_debug(2,"leaf-substmt -> reference-stmt"); }
              | unknown_stmt         { clicon_debug(2,"leaf-substmt -> unknown-stmt");} 
              |                      { clicon_debug(2,"leaf-substmt ->"); }
              ;

/* leaf-list */
leaf_list_stmt : K_LEAF_LIST id_arg_str ';'
              | K_LEAF_LIST id_arg_str '{' leaf_list_substmts '}'
              ;

leaf_list_substmts : leaf_list_substmts leaf_list_substmt
              | leaf_list_substmt
              ;

leaf_list_substmt : type_stmt        { clicon_debug(2,"leaf-list-substmt -> type-stmt"); }
              | description_stmt     { clicon_debug(2,"leaf-list-substmt -> description-stmt"); }
              | reference_stmt       { clicon_debug(2,"leaf-list-substmt -> reference-stmt"); }
              | unknown_stmt         { clicon_debug(2,"leaf-list-substmt -> unknown-stmt");} 
              |                      { clicon_debug(2,"leaf-list-stmt ->"); }
              ;

/* list */
list_stmt     : K_LIST id_arg_str ';' 
                      { clicon_debug(2,"list-stmt -> LIST id-ar-str ;"); }
              | K_LIST id_arg_str '{' list_substmts '}' 
                      { clicon_debug(2,"list-stmt -> LIST id-ar-str { list-substmts }"); }
              ;

list_substmts : list_substmts list_substmt 
                      { clicon_debug(2,"list-substmts -> list-substmts list-substmt"); }
              | list_substmt 
                      { clicon_debug(2,"list-substmts -> list-substmt"); }
              ;

list_substmt  : data_def_stmt        { clicon_debug(2,"list-substmt -> data-def-stmt"); }
              | key_stmt             { clicon_debug(2,"list-substmt -> key-stmt"); }
              | description_stmt     { clicon_debug(2,"list-substmt -> description-stmt"); }
              | reference_stmt       { clicon_debug(2,"list-substmt -> reference-stmt"); }
              | unknown_stmt         { clicon_debug(2,"list-substmt -> unknown-stmt");} 
              |                      { clicon_debug(2,"list-substmt -> "); }
              ;

/* Simple statements */
yang_version_stmt  : K_YANG_VERSION string ';' /* XXX yang-version-arg-str */
                          { clicon_debug(2,"yang-version-stmt -> YANG_VERSION string"); }
              ;

contact_stmt  : K_CONTACT string ';'
                          { clicon_debug(2,"contact-stmt -> CONTACT string"); }
              ;

import_stmt   : K_IMPORT id_arg_str '{' prefix_stmt '}' 
                          { clicon_debug(2,"import-stmt -> id-arg-str { prefix-stmt }"); }
              ;
include_stmt  : K_INCLUDE id_arg_str ';'
                          { clicon_debug(2,"include-stmt -> id-arg-str"); }
              ;

namespace_stmt : K_NAMESPACE string ';'  /* XXX uri-str */
              ;

prefix_stmt   : K_PREFIX string ';' /* XXX prefix-arg-str */
                       { clicon_debug(2,"prefix-stmt -> PREFIX string ;");}
              ;

description_stmt: K_DESCRIPTION string ';' /* XXX identifier_ref_arg_str */
                       { clicon_debug(2,"description-stmt -> DESCRIPTION string ;");}
              ;

organization_stmt: K_ORGANIZATION string ';'
                       { clicon_debug(2,"organization-stmt -> ORGANIZATION string ;");}
              ;

reference_stmt: K_REFERENCE string ';'
                       { clicon_debug(2,"reference-stmt -> REFERENCE string ;");}
              ;

type_stmt     : K_TYPE id_arg_str ';' /* XXX identifier_ref_arg_str */
              ;

key_stmt      : K_KEY id_arg_str ';' /* XXX key_arg_str */
              ;

id_arg_str    : string { $$=$1; clicon_debug(2,"id-arg-str -> string"); }
              ;	      

string        : qstring { $$=$1; clicon_debug(2,"string -> qstring (%s)", $1); }
              | ustring { $$=$1; clicon_debug(2,"string -> ustring (%s)", $1); }
              ;	      

/* quoted string */
qstring        : DQ ustring DQ  { $$=$2; clicon_debug(2,"string-> \" ustring \""); } 
  ;

/* unquoted string */
ustring       : ustring CHAR {
		  int len = strlen($1);
		  $$ = realloc($1, len+strlen($2) +1); 
		  sprintf($$+len, "%s", $2); 
		  free($2);
	      }
              | CHAR {$$=$1; } 
              ;



%%

