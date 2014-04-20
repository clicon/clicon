/*
 *  CVS Version: $Id: clicon_dbspec.y,v 1.4 2013/09/18 19:14:53 olof Exp $
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

 * Database specification parser cli syntax
 * Cloned from cligen parser. But made the following changes:
 * - removed callbacks and flags
 */

%start file

%union {
  int intval;
  char *string;
}

%token MY_EOF
%token V_TYPE V_RANGE V_CHOICE V_KEYWORD V_REGEXP V_DEFAULT 
%token V_MANDATORY V_OPTIONAL
%token DOUBLEPARENT /* (( */
%token DQ           /* " */
%token DQP          /* ") */
%token PDQ          /* (" */

%token <string> NAME /* in variables: <NAME type:NAME> */
%token <string> NUMBER /* In variables */
%token <string> CHAR

%type <string> charseq
%type <string> choices
%type <string> varname

%lex-param     {void *_ya} /* Add this argument to parse() and lex() function */
%parse-param   {void *_ya}

%{
/* Here starts user C-code */

/* typecast macro */
#define _YA ((struct clicon_dbspec_yacc_arg *)_ya)

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
#include "clicon_dbspec.h"

extern int clicon_dbspecget_lineno  (void);

int 
clicon_dbspec_debug(int d)
{
    debug = d;
    return 0;
}


/* 
   clicon_dbspecerror
   also called from yacc generated code *
*/
void clicon_dbspecerror(void *_ya, char *s) 
{ 
  fprintf(stderr, "%s%s%d: Error: %s: at or before: '%s'\n", 
	  _YA->ya_name,
	   ":" ,
	  _YA->ya_linenum ,
	  s, 
	  clicon_dbspectext); 
  return;
}


/* 
 * create_cv
 * Create a CLIgen variable (cv) and store it in the current variable object 
 * Note that only one such cv can be stored.
 */
static cg_var *
create_cv(struct clicon_dbspec_yacc_arg *ya, char *name, char *type, char *str)
{
    cg_var             *cv = NULL;

    if ((cv = cv_new(CGV_STRING)) == NULL){
	fprintf(stderr, "malloc: %s\n", strerror(errno));
	goto done;
    }
    if (type){
	if (cv_type_set(cv, cv_str2type(type)) == CGV_ERR){
	    fprintf(stderr, "%s:%d: error: No such type: %s\n",
		    ya->ya_name, ya->ya_linenum, type);
	    cv_free(cv); cv = NULL;
	    goto done;
	}
    }
    if (name){
	if (cv_name_set(cv, name) < 0){
	    fprintf(stderr, "%s: error: cv_name_set()\n",
		    ya->ya_name);
	    cv_free(cv); cv = NULL;
	    goto done;
	}
    }
    if (cv_parse(str, cv) < 0){ /* parse str into cgv */
	cv_free(cv); cv = NULL;
	goto done;
    }
  done:
    return cv;
}

/*
 * Only string type supported for now
 */
static int
dbs_assignment(struct clicon_dbspec_yacc_arg *ya, char *var, char *val)
{
    struct dbs_stack *cs = ya->ya_stack;
    int              retval = -1;
    cg_var          *cv;

    if (cs == NULL){
	fprintf(stderr, "%s: Error, stack should not be NULL\n", __FUNCTION__);
    }
     if (cs->cs_next != NULL){ /* local */
	 if (ya->ya_cvec == NULL)
	     if ((ya->ya_cvec = cvec_new(0)) == NULL){
		 fprintf(stderr, "%s: cvec_new:%s\n", __FUNCTION__, strerror(errno));
		 goto done;
	     }
	 if ((cv = cvec_add(ya->ya_cvec, CGV_STRING)) == NULL){
	    fprintf(stderr, "%s: realloc:%s\n", __FUNCTION__, strerror(errno));
	    goto done;
	}
	cv_name_set(cv, var);
	if (cv_parse(val, cv) < 0)
	    goto done;
    }
    else{ /* global */
	if ((cv = cvec_add(ya->ya_globals, CGV_STRING)) == NULL){
	    fprintf(stderr, "%s: realloc:%s\n", __FUNCTION__, strerror(errno));
	    goto done;
	}
	cv_name_set(cv, var);
	if (cv_parse(val, cv) < 0)  /* May be wrong type */
	    goto done;
    }
    retval = 0;
  done:
    return retval;
}


static int
expand_arg(struct clicon_dbspec_yacc_arg *ya, char *type, char *arg)
{
    int                 retval = -1;
    cg_var             *cgv;
    cg_obj             *co;

    co = ya->ya_var;
    if ((cgv = create_cv(ya, co->co_command, type, arg)) == NULL)
	goto done;
    ya->ya_var->co_expand_fn_arg = cgv;
    retval = 0;
  done:
    return retval;
}

static int
dbs_list_push(cg_obj *co, struct dbs_list **cl0)
{
    struct dbs_list *cl;

    if ((cl = malloc(sizeof(*cl))) == NULL) {
	fprintf(stderr, "%s: malloc: %s\n", __FUNCTION__, strerror(errno));
	return -1;
    }
    cl->cl_next = *cl0;
    cl->cl_obj = co;
    *cl0 = cl;
    return 0;
}

/* Delet whole list */
static int
dbs_list_delete(struct dbs_list **cl0)
{
    struct dbs_list *cl;

    while ((cl = *cl0) != NULL){
	*cl0 = cl->cl_next;
	free(cl);
    }
    return 0;
}


/* 
 * Create new tmp variable cligen object 
 * It must be filled in by later functions.
 * The reason is, the variable must be completely parsed by successive syntax
 * (eg <type:> stuff) and we cant insert it into the object tree until that is done.
 * And completed by the '_post' function
 * Returns: tmp variable object
 */
static cg_obj *
dbs_var_pre(struct clicon_dbspec_yacc_arg *ya)
{
    cg_obj *co_new;

    if ((co_new = malloc(sizeof(cg_obj))) == NULL){
	fprintf(stderr, "%s: malloc: %s\n", __FUNCTION__, strerror(errno));
	clicon_dbspecerror(ya, "Allocating cligen object"); 
	return NULL;
    }
    memset(co_new, 0, sizeof(cg_obj));
    co_new->co_type      = CO_VARIABLE;
    co_new->co_max       = 0;
    if (dbspec_userdata_add(ya->ya_handle, co_new) < 0){
	clicon_dbspecerror(ya, "Allocating cligen object"); 
	return NULL;
    }

    return co_new;
}

static int
validate_name(const char *name)
{
    if (!isalpha(*name) && *name!='_') /* Must beging with [a-zA-Z_] */
	return 0;

    while (*(++name)) 
	if (!isalnum(*name) && *name!='_')
	    return 0;
		
    return 1;
}

/* 
 * Complete variable cligen object after parsing is complete and insert it into
 * object hierarchies. That is, insert a variable in each hieracrhy.
 * Returns:
 *       new object or sister that is placed in the hierarchy.
 * Arguments:
 *       cv     - tmp variable object
 */
static int
dbs_var_post(struct clicon_dbspec_yacc_arg *ya)
{
    struct dbs_list *cl; 
    cg_obj *cv1; /* variable copy object */
    cg_obj *cop; /* parent */
    cg_obj *co;  /* new obj/sister */
    cg_obj *cv = ya->ya_var;

    if (cv->co_vtype == CGV_ERR) /* unassigned */
	cv->co_vtype = cv_str2type(cv->co_command);
    clicon_debug(2, "%s: cmd:%s vtype:%d", __FUNCTION__, 
		cv->co_command,
		cv->co_vtype );
    if (cv->co_vtype == CGV_ERR){
	clicon_dbspecerror(ya, "Wrong or unassigned variable type"); 	
	return -1;
    }
#if 0 /* XXX dont really know what i am doing but variables dont behave nice in choice */
    if (ya->ya_opt){     /* get cop from stack */
	if (ya->ya_stack == NULL){
	    fprintf(stderr, "Option allowed only within () or []\n");
	    return -1;
	}
	cl = ya->ya_stack->cs_list;
    }
    else
#endif
	cl = ya->ya_list;
    for (; cl; cl = cl->cl_next){
	cop = cl->cl_obj;
	if (cl->cl_next){
	    if (co_copy(cv, cop, &cv1) < 0)
		return -1;
	}
	else
	    cv1 = cv; /* Dont copy if last in list */
	co_up_set(cv1, cop);
	if ((co = co_insert(&cop->co_pt, cv1)) == NULL) /* co_new may be deleted */
	    return -1;
	cl->cl_obj = co;
    }
    return 0;
}

/*
 * Create a new command object. Actually, create a new for every tree in the list
 * and replace the old with the new object.
 * Returns:
 *  -1 on error 0 on OK
 * Arguments:
 *   cmd: the command string
 */
static int
dbs_cmd(struct clicon_dbspec_yacc_arg *ya, char *cmd, char *indexvar)
{
    struct dbs_list *cl; 
    cg_obj *cop; /* parent */
    cg_obj *conew; /* new obj */
    cg_obj *co; /* new/sister */

    for (cl=ya->ya_list; cl; cl = cl->cl_next){
	cop = cl->cl_obj;
	clicon_debug(2, "%s: %s parent:%s",
		    __FUNCTION__, cmd, cop->co_command);
	if ((conew = co_new(cmd, cop)) == NULL) { 
	    clicon_dbspecerror(ya, "Allocating cligen object"); 
	    return -1;
	}
	if (dbspec_userdata_add(ya->ya_handle, conew) < 0){
	    clicon_dbspecerror(ya, "Allocating cligen object"); 
	    return -1;
	}

	if (indexvar != NULL)
	    if (dbspec_indexvar_set(conew, strdup(indexvar)) < 0){
		clicon_dbspecerror(ya, "strdup"); 
		return -1;
	    }
	if ((co = co_insert(&cop->co_pt, conew)) == NULL)  /* co_new may be deleted */
	    return -1;
	cl->cl_obj = co; /* Replace parent in dbs_list */
    }
    return 0;
}

/* 
   dbs_reference
   Create a REFERENCE node that references another tree.
   This is evaluated in runtime by pt_expand().
   See also db2tree() in clicon/apps/cli_main.c on how to create such a tree
   And pt_expand_1()/pt_callback_reference() how it is expanded
   And
 */
static int
dbs_reference(struct clicon_dbspec_yacc_arg *ya, char *name)
{
    struct dbs_list *cl; 
    cg_obj          *cop;   /* parent */
    cg_obj          *cot;   /* tree */

    for (cl=ya->ya_list; cl; cl = cl->cl_next){
	/* Add a treeref 'stub' which is expanded in pt_expand to a sub-tree */
	cop = cl->cl_obj;
	if ((cot = co_new(name, cop)) == NULL) { 
	    clicon_dbspecerror(ya, "Allocating cligen object"); 
	    return -1;
	}
	if (dbspec_userdata_add(ya->ya_handle, cot) < 0){
	    clicon_dbspecerror(ya, "Allocating cligen object"); 
	    return -1;
	}

	cot->co_type    = CO_REFERENCE;
	if (co_insert(&cop->co_pt, cot) == NULL)  /* cot may be deleted */
	    return -1;
	/* Replace parent in dbs_list: not allowed after ref?
	   but only way to add callbacks to it.
	*/
	cl->cl_obj = cot;
   }
 
    return 0;
}


/* assume comment is malloced and not freed by parser */
static int
dbs_comment(struct clicon_dbspec_yacc_arg *ya, char *comment)
{
    struct dbs_list *cl; 
    cg_obj *co; 

    for (cl = ya->ya_list; cl; cl = cl->cl_next){
	co = cl->cl_obj;
	if (co->co_help == NULL) /* Why would it already have a comment? */
	    if ((co->co_help = strdup(comment)) == NULL){
		clicon_dbspecerror(ya, "Allocating comment"); 
		return -1;
	    }
    }
    return 0;
}

static char *
dbs_choice_merge(struct clicon_dbspec_yacc_arg *ya, char *str, char *app)
{
    int len;
    char *s;

    len = strlen(str)+strlen(app) + 2;
    if ((s = realloc(str, len)) == NULL) {
	fprintf(stderr, "%s: realloc: %s\n", __FUNCTION__, strerror(errno));
	return NULL;
    }
    strncat(s, "|", len-1);
    strncat(s, app, len-1);
    return s;
}

/*
 * Post-processing of commands, eg at ';':
 *  a cmd;<--
 * But only if parsing succesful.
 * 1. Add callback and args to every list
 * 2. Add empty child unless already empty child
 */
int
dbs_terminal(struct clicon_dbspec_yacc_arg *ya)
{
    struct dbs_list    *cl; 
    cg_obj             *co; 
    int                 i;
    int                 retval = -1;
    
    for (cl = ya->ya_list; cl; cl = cl->cl_next){
	co  = cl->cl_obj;
		
	if (ya->ya_cvec){
#ifdef notyet /* XXX: where did auth code go? */
	    if ((cv = cvec_find_var(ya->ya_cvec, "auth")) != NULL)
		co->co_auth = strdup(cv_string_get(cv));
#endif
	    if (cvec_find_var(ya->ya_cvec, "hide") != NULL)
		co->co_hide = 1;
	    /* generic variables */
	    if ((co->co_cvec = cvec_dup(ya->ya_cvec)) == NULL){
		fprintf(stderr, "%s: cvec_dup: %s\n", __FUNCTION__, strerror(errno));
		goto done;
	    }
	}
	/* misc */
	for (i=0; i<co->co_max; i++)
	    if (co->co_next[i]==NULL)
		break;
	if (i == co->co_max) /* Insert empty child if ';' */
	    co_insert(&co->co_pt, NULL);
    }
    /* cleanup */
    if (ya->ya_cvec){
	cvec_free(ya->ya_cvec);
	ya->ya_cvec = NULL;
    }
    retval = 0;
  done:
    return retval;  
}

/*
 * Take the whole dbs_list and push it to the stack 
 */
static int
ctx_push(struct clicon_dbspec_yacc_arg *ya)
{
    struct dbs_list *cl; 
    struct dbs_stack *cs; 

    if ((cs = malloc(sizeof(*cs))) == NULL) {
	fprintf(stderr, "%s: malloc: %s\n", __FUNCTION__, strerror(errno));
	return -1;
    }
    memset(cs, 0, sizeof(*cs));
    cs->cs_next = ya->ya_stack;
    ya->ya_stack = cs;
    for (cl = ya->ya_list; cl; cl = cl->cl_next){
    	if (dbs_list_push(cl->cl_obj, &cs->cs_list) < 0) 
	    return -1;
    }
    return 0;
}

/*
 * ctx_peek_swap
 * Peek context from stack and replace the object list with it
 * Typically done in a choice, eg "(c1|c2)" at c2.
 * Dont pop context
  */
static int
ctx_peek_swap(struct clicon_dbspec_yacc_arg *ya)
{
    struct dbs_stack *cs; 
    struct dbs_list *cl; 
    cg_obj *co; 

    if ((cs = ya->ya_stack) == NULL){
#if 1
	clicon_dbspecerror(ya, "No surrounding () or []"); 
	return -1; /* e.g a|b instead of (a|b) */
#else
	dbs_list_delete(&ya->ya_list);
	return 0;
#endif
    }
    for (cl = ya->ya_list; cl; cl = cl->cl_next){
	co = cl->cl_obj;
	if (dbs_list_push(co, &cs->cs_saved) < 0)
	    return -1;
    }
    dbs_list_delete(&ya->ya_list);
    for (cl = cs->cs_list; cl; cl = cl->cl_next){
	co = cl->cl_obj;
	if (dbs_list_push(co, &ya->ya_list) < 0)
	    return -1;
    }
    return 0;
}

/*
 * ctx_peek_swap2
 * Peek context from stack and replace the object list with it
 * Typically done in a choice, eg "(c1|c2)" at c2.
 * Dont pop context
  */
static int
ctx_peek_swap2(struct clicon_dbspec_yacc_arg *ya)
{
    struct dbs_stack *cs; 
    struct dbs_list  *cl; 
    cg_obj           *co; 

    if ((cs = ya->ya_stack) == NULL){
#if 1
	clicon_dbspecerror(ya, "No surrounding () or []"); 
	return -1; /* e.g a|b instead of (a|b) */
#else
	dbs_list_delete(&ya->ya_list);
	return 0;
#endif
    }
    dbs_list_delete(&ya->ya_list);
    for (cl = cs->cs_list; cl; cl = cl->cl_next){
	co = cl->cl_obj;
	if (dbs_list_push(co, &ya->ya_list) < 0)
	    return -1;
    }
    return 0;
}

static int
delete_stack_element(struct dbs_stack *cs)
{
    dbs_list_delete(&cs->cs_list);
    dbs_list_delete(&cs->cs_saved);
    free(cs);

    return 0;
}


/*
 * ctx_pop
 * Pop context from stack and discard it.
 * Typically done after a grouping, eg "cmd (opt1|opt2)"
 */
static int
ctx_pop(struct clicon_dbspec_yacc_arg *ya)
{
    struct dbs_stack *cs; 
    struct dbs_list *cl; 
    cg_obj *co; 

    if ((cs = ya->ya_stack) == NULL){
	fprintf(stderr, "%s: dbs_stack empty\n", __FUNCTION__);
	return -1; /* shouldnt happen */
    }
    ya->ya_stack = cs->cs_next;
    for (cl = cs->cs_saved; cl; cl = cl->cl_next){
	co = cl->cl_obj;
	if (dbs_list_push(co, &ya->ya_list) < 0)
	    return -1;
    }
    delete_stack_element(cs);
    return 0;
}

static int
cg_regexp(struct clicon_dbspec_yacc_arg *ya, char *rx)
{
    ya->ya_var->co_regex = rx;  
    ya->ya_var->co_vtype=CGV_STRING;
    return 0;
}

static int
cg_default(struct clicon_dbspec_yacc_arg *ya, char *type, char *val)
{
    cg_var          *cv; 
    cg_obj             *co;

    co = ya->ya_var;
    if ((cv = create_cv(ya, co->co_command, type, val)) == NULL){
	clicon_dbspecerror(ya, "error when creating default value"); 
	return -1;
    }
    if (dbspec_default_get(ya->ya_var))
	cv_free(dbspec_default_get(ya->ya_var));
    dbspec_default_set(ya->ya_var, cv);
    return 0;
}

static int
cg_range(struct clicon_dbspec_yacc_arg *ya, char *low, char *high)
{
    int   retval;
    char *reason = NULL;

    if ((retval = parse_int64(low, &ya->ya_var->co_range_low, &reason)) < 0){
	fprintf(stderr, "range: %s\n", strerror(errno));
	return -1;
    }
    if (retval == 0){
	clicon_dbspecerror(ya, reason); 
	return 0;
    }
    if ((retval = parse_int64(high, &ya->ya_var->co_range_high, &reason)) < 0){
	fprintf(stderr, "range: %s\n", strerror(errno));
	return -1;
    }
    if (retval == 0){
	clicon_dbspecerror(ya, reason); 
	return 0;
    }
    ya->ya_var->co_range++;
    return 0;
}

int
dbspec_parse_init(struct clicon_dbspec_yacc_arg *ya, cg_obj *co_top)
{
    /* Add top-level object */
    if (dbs_list_push(co_top, &ya->ya_list) < 0)
	return -1;
    if (ctx_push(ya) < 0)
	return -1;
    return 0;
}

int
dbspec_parse_exit(struct clicon_dbspec_yacc_arg *ya)
{
    struct dbs_stack *cs; 

    ya->ya_var = NULL;
    dbs_list_delete(&ya->ya_list);
    if((cs = ya->ya_stack) != NULL){
	delete_stack_element(cs);
#if 0
	fprintf(stderr, "%s:%d: error: lacking () or [] at or before: '%s'\n", 
		ya->ya_name,
		ya->ya_linenum,
		ya->ya_parse_string
	    );
	return -1;
#endif
    }
    return 0;
}


%} 
 
%%

file          : lines MY_EOF{clicon_debug(2,"file->lines"); YYACCEPT;} 
              | MY_EOF {clicon_debug(2,"file->"); YYACCEPT;} 
              ;

lines        : lines line {
                  clicon_debug(2,"lines->lines line");
                 } 
              | line  { clicon_debug(2,"lines->line"); } 
              ;

line          : decltop line2	{ clicon_debug(2, "line->decltop line2"); }	
              | assignment ';'  { clicon_debug(2, "line->assignment ;"); }
              ;

assignment    : NAME '=' DQ charseq DQ {dbs_assignment(_ya, $1,$4);free($1); free($4);}
              ; 

line2        : ';' { clicon_debug(2, "line2->';'"); if (dbs_terminal(_ya) < 0) YYERROR;if (ctx_peek_swap2(_ya) < 0) YYERROR; } 
              | '{' '}' { clicon_debug(2, "line2->'{' '}'"); }
              | '{' {if (ctx_push(_ya) < 0) YYERROR; } 
                lines 
                '}' { clicon_debug(2, "line2->'{' lines '}'");if (ctx_pop(_ya) < 0) YYERROR;if (ctx_peek_swap2(_ya) < 0) YYERROR; }
              | ';' { if (dbs_terminal(_ya) < 0) YYERROR; } 
                '{' { if (ctx_push(_ya) < 0) YYERROR; }
                lines 
                '}' { clicon_debug(2, "line2->';' '{' lines '}'");if (ctx_pop(_ya) < 0) YYERROR;if (ctx_peek_swap2(_ya) < 0) YYERROR; }
              ;

decltop        : decllist  {clicon_debug(2, "decltop->decllist");}
               | declcomp  {clicon_debug(2, "decltop->declcomp");}
               ;

decllist      : decltop 
                declcomp  {clicon_debug(2, "decllist->decltop declcomp");}
              | decltop '|' { if (ctx_peek_swap(_ya) < 0) YYERROR;} 
                declcomp  {clicon_debug(2, "decllist->decltop | declcomp");}
              ;

declcomp      : '(' { if (ctx_push(_ya) < 0) YYERROR; } decltop ')' { if (ctx_pop(_ya) < 0) YYERROR; clicon_debug(2, "declcomp->(decltop)");}
              | decl  {clicon_debug(2, "declcomp->decl");}
              ;

decl        : cmd {clicon_debug(2, "decl->cmd");}
            | cmd PDQ charseq DQP { clicon_debug(2, "decl->cmd (\" comment \")");if (dbs_comment(_ya, $3) < 0) YYERROR; free($3);}
            | cmd PDQ DQP { clicon_debug(2, "decl->cmd (\"\")");}
            ;

cmd         : NAME { clicon_debug(2, "cmd->NAME");if (dbs_cmd(_ya, $1, NULL) < 0) YYERROR; free($1); } 
            | NAME '[' NAME ']' { clicon_debug(2, "cmd->[NAME]");if (dbs_cmd(_ya, $1, $3) < 0) YYERROR; free($1); free($3);} 
            | '@' NAME { clicon_debug(2, "cmd->@NAME");if (dbs_reference(_ya, $2) < 0) YYERROR; free($2); } 
            | '<' { if ((_YA->ya_var = dbs_var_pre(_YA)) == NULL) YYERROR; }
               variable '>'  { if (dbs_var_post(_ya) < 0) YYERROR; }
            ;

variable    : varname { _YA->ya_var->co_command = $1;}
            | varname ':' NAME{ 
		_YA->ya_var->co_command = $1; 
		_YA->ya_var->co_vtype = cv_str2type($3); free($3);
	       }
            | varname ' ' keypairs { _YA->ya_var->co_command = $1; }
            | varname ':' NAME ' ' { 
		 _YA->ya_var->co_command = $1; 
		 _YA->ya_var->co_vtype = cv_str2type($3); free($3); 
               } keypairs
            ;

varname     : NAME { if (validate_name($1)==0){	clicon_dbspecerror(_YA, "Invalid variable name"); YYERROR;} $$ = $1;}
            | NAME '[' ']'{ if (validate_name($1)==0) {	clicon_dbspecerror(_YA, "Invalid variable name"); YYERROR;}; $$ = $1; dbspec_vector_set(_YA->ya_var, 1); }
            ;

keypairs    : keypair 
            | keypairs ' ' keypair
            ;

keypair     : NAME '(' ')' { _YA->ya_var->co_expand_fn_str = $1; }
            | NAME '(' DQ DQ ')' {_YA->ya_var->co_expand_fn_str = $1; }
            | NAME '(' DQ charseq DQ ')' {
		_YA->ya_var->co_expand_fn_str = $1; 
		expand_arg(_ya, "string", $4);
		free($4); 
	      }
            | V_RANGE '[' NUMBER ':' NUMBER ']' { 
		if (cg_range(_ya, $3, $5) < 0) YYERROR; ; 
	      }
            | V_MANDATORY { dbspec_optional_set(_YA->ya_var, 0);}
            | V_OPTIONAL { dbspec_optional_set(_YA->ya_var, 1);}
            | V_CHOICE ':' choices { _YA->ya_var->co_choice = $3; }
            | V_KEYWORD ':' NAME { 
		_YA->ya_var->co_keyword = $3;  
		_YA->ya_var->co_vtype=CGV_STRING; 
	      }
            | V_REGEXP  ':' DQ charseq DQ { cg_regexp(_ya, $4); }
            | V_DEFAULT ':' DQ charseq DQ { cg_default(_ya, NULL, $4); free($4);}
            | V_DEFAULT ':' '(' NAME ')' DQ charseq DQ { cg_default(_ya, $4, $7); free($4); free($7);}
            | V_DEFAULT ':' '(' NAME ')' NUMBER { cg_default(_ya, $4, $6); free($4); free($6);}
            | V_DEFAULT ':' '(' NAME ')' NAME { cg_default(_ya, $4, $6); free($4); free($6);}

            ;

choices     : NUMBER { $$ = $1;}
            | NAME { $$ = $1;}
            | choices '|' NUMBER { $$ = dbs_choice_merge(_ya, $1, $3); free($3);}
            | choices '|' NAME { $$ = dbs_choice_merge(_ya, $1, $3); free($3);}
            ;

charseq    : charseq CHAR 
              {
		  int len = strlen($1);
		  $$ = realloc($1, len+strlen($2) +1); 
		  sprintf($$+len, "%s", $2); 
		  free($2);
                 }

           | CHAR {$$=$1;}
           ;


%%

