/*
 *  CVS Version: $Id: clicon_db2txt.y,v 1.10 2013/09/11 18:36:46 olof Exp $
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
 */

%{
#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include "clicon_err.h"
#include "clicon_queue.h"
#include "clicon_hash.h"
#include "clicon_chunk.h"
#include "clicon_file.h"
#include "clicon_handle.h"
#include "clicon_spec.h"
#include "clicon_lvalue.h"
#include "clicon_options.h"
#include "clicon_plugin.h"
#include "clicon_dbutil.h"
#include "clicon_db2txt.h"
#include "clicon_db2txt.y.h"
#include "clicon_db2txt.tab.h"

#if 0
/* add _yf to yacc parse function parameters */
#define YYPARSE_PARAM _ya

/* add _yf to lex parse function parameters */
#define YYLEX_PARAM _ya
#endif /* 0 */

/* typecast macro */
#define _YA ((db2txt_t *)_ya)

/* add _yf to error paramaters */
//#define YY_(msgid) _YA, msgid 
#define YY_(msgid) msgid 

/* booleans */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

%}

%union 
{
        char character;
        int number;
        char *string;
        keyvec_t *keyvec;
    	cg_var *cv;
	cmpfn_t *cmp;

}

%token EOL
%token WHITE
%token <string> NAME

%lex-param     {void *_ya} /* Add this argument to parse() and lex() function */
%parse-param   {void *_ya}


%{

/*
 * 
 */
static int
db2txt_out(void *_ya, char *str)
{
    size_t len;
    char *new;
    code_stack_t *stack;

    stack = (code_stack_t *)_YA->ya_code_stack;
    if (stack && stack->processing == FALSE)
	return 0;
    db2txt_debug(str);

#if notyet
    if (_YA->ya_outcb)
	if (_YA->ya_outcb(str) != 0)
	    return -1;
#endif
    if (_YA->ya_ret) {
	len = strlen(_YA->ya_ret);
	if ((new = realloc(_YA->ya_ret, len + strlen(str) + 1)) == NULL) {
	    clicon_err(OE_UNIX, errno, "realloc");
	    return -1;
	}
	sprintf(new+len, "%s", str);	
	_YA->ya_ret = new;
    }

    return 0;
}

static cg_var *
parse_val(enum cv_type type, char *val)
{
    cg_var *cv;
    
    if(type == CGV_ERR) {
	clicon_err(OE_CFG, 0, "Invalid type: %d", type);
	return NULL;
    }
    if ((cv = cv_new(type)) == NULL) {
	clicon_err(OE_UNIX, errno, "cv_new");
	return NULL;
    }
    if (cv_parse(val, cv) != 0) 
	return NULL;

    return cv;
}


static cg_var *
get_nullvar(void *_ya)
{
    cg_var *cv;

    if ((cv = cv_dup(_YA->ya_null)) == NULL) {
	clicon_db2txterror(_YA, "malloc failed");
	return NULL;
    }

    return cv;
}

static char *
get_loopkey(void *_ya, char *key)
{
    char savec = 0xff;
    char *rest;
    char *new = NULL;
    code_stack_t *cs;
    code_stack_t *theone = NULL;

    if (_YA->ya_code_stack == NULL)
	return NULL;
	
    /* Find loop variable part. Can only be [a-zA-Z0-9_]+ */
    rest = key;
    while (isalnum(*rest) || *rest == '_')
	rest++;
    savec = *rest;
    *rest = '\0';

    /* Look for loop variable in  code stack */
    cs = (code_stack_t *)_YA->ya_code_stack;
    do { 
	if (cs->loopvar && strcmp(cs->loopvar, key) == 0) { /* found it */
	    theone = cs;
	    break;
	}
	cs = NEXTQ(code_stack_t *, cs);
    } while (cs != (code_stack_t *)_YA->ya_code_stack);
    *rest = savec; /* Restore saved character */

    if (theone == NULL) 
	return NULL;

    if (cs->keyvec->idx >= cs->keyvec->len)
	return NULL;
    
    new = malloc(strlen(cs->keyvec->vec[cs->keyvec->idx]) + strlen(rest) +1);
    if (new == NULL) {
	clicon_db2txterror(_ya, "malloc failed");
	return NULL;
    }
    sprintf(new, "%s%s", cs->keyvec->vec[cs->keyvec->idx], rest);

    return new;
}
    
static cg_var *
get_dbvar(void *_ya, char *varstr)
{
    char *key;
    char *var;
    char *new = NULL;
    cg_var *cv;

    key = varstr;
    if ((var = strstr(varstr, "->")) == NULL) {
	clicon_db2txterror(_ya, "Invalid database variable format");
	return NULL;
    }
    *var = '\0';
    var += 2;

    /* First look for an each vector variable */
    if ((new = get_loopkey(_ya, key)) != NULL)
	key = new;

    cv = dbvar2cv(_YA->ya_db, key, var);
    if (new)
	free(new);
    if (cv == NULL && (cv = get_nullvar(_ya)) == NULL)
	return NULL;
    
    return cv;
}

static int
cmp_true(cg_var *cv)
{
    extern char cv_bool_get(cg_var *cv);

    switch(cv_type_get(cv)) {
    case CGV_ERR:
	return FALSE;
    case CGV_INT:
	return (cv_int_get(cv) != 0);
    case CGV_LONG:
	return (cv_long_get(cv) != 0);
    case CGV_BOOL:
	return (cv_bool_get(cv) != 0);
    default:
	return TRUE;
    }
}

static int
cmp_eq(cg_var *cv1, cg_var *cv2)
{
    if (cv_type_get(cv1) == CGV_ERR && cv_type_get(cv2) == CGV_ERR)
	return 1;
    return cv_cmp(cv1, cv2) == 0;
}

static int
cmp_neq(cg_var *cv1, cg_var *cv2)
{
    if (cv_type_get(cv1) == CGV_ERR && cv_type_get(cv2) == CGV_ERR)
	return 0;
    return (cv_cmp(cv1, cv2) != 0);
}

static int
cmp_gt(cg_var *cv1, cg_var *cv2)
{
    if (cv_type_get(cv1) == CGV_ERR && cv_type_get(cv2) == CGV_ERR)
	return 0;
    return (cv_cmp(cv1, cv2) > 0);
}

static int
cmp_lt(cg_var *cv1, cg_var *cv2)
{
    if (cv_type_get(cv1) == CGV_ERR && cv_type_get(cv2) == CGV_ERR)
	return 0;
    return (cv_cmp(cv1, cv2) < 0);
}

static int
cmp_gte(cg_var *cv1, cg_var *cv2)
{
    if (cv_type_get(cv1) == CGV_ERR && cv_type_get(cv2) == CGV_ERR)
	return 1;
    return (cv_cmp(cv1, cv2) >= 0);
}

static int
cmp_lte(cg_var *cv1, cg_var *cv2)
{
    if (cv_type_get(cv1) == CGV_ERR && cv_type_get(cv2) == CGV_ERR)
	return 1;
    return (cv_cmp(cv1, cv2) <= 0);
}

char *
db2txt_code_string(code_stack_t *cs)
{
    if (cs == NULL)
	return "NIL";

    switch(cs->codetype) {
    case INCLUDE:
	return "INCLUDE";
    case EACH:
	return "EACH";
    case EACH_INLINE:
	return "EACH_INLINE";
    case IF:
	return "IF";
    case ELSE:
	return "ELSE";
    default:
	return "Other";
    }
}

static code_stack_t *
push_code(void *_ya, int processing, int codetype)
{
    code_stack_t *stack;
    code_stack_t *cs;
    
    stack = (code_stack_t *)_YA->ya_code_stack;

    if ((cs = malloc(sizeof (code_stack_t))) == NULL) {
	clicon_db2txterror(_YA, "malloc failed");
	return NULL;
    }
    memset(cs, 0, sizeof(code_stack_t));

    cs->startline = _YA->ya_file_stack->line;
    if (stack && stack->processing == FALSE)    /* inherit non-processing */
	cs->processing = stack->processing;
    else
	cs->processing = processing;
    cs->codetype = codetype;

    db2txt_debug("PUSH code %s (%p) => %s (%p)\t at '%s' line %d\n",
		 db2txt_code_string((code_stack_t *)_YA->ya_code_stack),
		 (code_stack_t *)_YA->ya_code_stack,
		 db2txt_code_string(cs), cs,
		_YA->ya_file_stack->name ? _YA->ya_file_stack->name : "_buffer_",
		 _YA->ya_file_stack->line);
    INSQ(cs, _YA->ya_code_stack);
    
    return cs;
}

static int
pop_code(void *_ya)
{
    code_stack_t *cs;

    cs = _YA->ya_code_stack;
    if (cs == NULL)
	return -1;

    DELQ(cs, _YA->ya_code_stack, code_stack_t *);
    db2txt_debug("POP code %s (%p) => %s (%p)\t at '%s' line %d\n",
		 db2txt_code_string(cs), cs,
		 db2txt_code_string((code_stack_t *)_YA->ya_code_stack),
		 (code_stack_t *)_YA->ya_code_stack,
		 _YA->ya_file_stack->name ? _YA->ya_file_stack->name : "_buffer_",
		 _YA->ya_file_stack->line);
    if (cs->keyvec) {
	free(cs->keyvec->vec);
	free(cs->keyvec);
    }
    if (cs->loopvar)
	free(cs->loopvar);
    if (cs->loopbuf)
	free(cs->loopbuf);
    free(cs);

    return 0;
}

static void
end_each(void *_ya)
{
    code_stack_t *cs, *parent;
    
    if ((cs = (code_stack_t *)_YA->ya_code_stack) == NULL ||
	(cs->codetype != EACH && cs->codetype != EACH_INLINE))
	return;
    
    if (cs->keyvec->len == 0) {
	pop_code(_YA);
	return;
    }
    
    if (cs->append_loopbuf == TRUE) { /* First lap */
	cs->append_loopbuf = FALSE;
	if ((parent = NEXTQ(code_stack_t *, cs)) != NULL && parent != cs)
	    cs->processing = parent->processing;
	else
	    cs->processing = TRUE;
    }
    else {
	db2txt_popbuf(_YA);
	cs->keyvec->idx++;
    }

    if (cs->keyvec->idx < cs->keyvec->len) {
	db2txt_pushtxt(_YA, cs->loopbuf);
	 _YA->ya_file_stack->line +=
	     cs->startline - _YA->ya_file_stack->line - 
	     (cs->codetype == EACH_INLINE ? 1 : 0);
    } else
	pop_code(_ya);
}

static char *
run_callback(void *_ya, char *func, cg_var *arg)
{
    char *fname;
    char *pname = NULL;
    clicon_db2txtcb_t *cb;
    
    /* Make copy */
    if ((func = strdup(func)) == NULL) {
	clicon_err(OE_UNIX, errno, "strdup");
	return NULL;
    }

    /* Extract plugin name if any */
    if ((fname = strstr(func, "::")) != NULL) {
	*fname = '\0';
	fname += 2;
	pname = func;
    }
    else
	fname = func;
    
    
    cb = (clicon_db2txtcb_t *)clicon_find_func(_YA->ya_handle, pname, fname);
    if (cb == NULL)
	return NULL;

    return cb(arg);
}

static keyvec_t *
get_vector(void *_ya, char *basekey)
{
    int len;
    keyvec_t *vec;
    char *dbkey;
    char *key;

    len = strlen(basekey)+3;
    if ((dbkey = malloc(len)) == NULL) {
	clicon_db2txterror(_YA, "malloc failed");
	return NULL;
    }
    snprintf(dbkey, len, "%s[]", basekey);

    if ((vec = malloc(sizeof(*vec))) == NULL) {
	clicon_db2txterror(_YA, "malloc failed");
	free(dbkey);
	return NULL;
    }
    memset(vec, 0, sizeof(*vec));
    
    if ((key = get_loopkey(_ya, dbkey)) == NULL)
	if ((key = strdup(dbkey)) == NULL) 
	    clicon_db2txterror(_YA, "malloc failed");
    free(dbkey);
    if (key == NULL)
	return NULL;
    
    vec->vec = dbvectorkeys(_YA->ya_db, key, &vec->len);
    free(key);
    if (vec->vec == NULL) {
	clicon_db2txterror(_YA, "dbvectorkeys failed");
	free(vec);
	return NULL;
    }
    return vec;
}

/*
 * Clean up parser structures
 * Called from clicon_db2txt.l.
 */
int
db2txt_parser_cleanup(void *_ya)
{
    return pop_code(_ya);
}

%}

%token MY_EOF
%token <string> EQ
%token <string> NEQ
%token <string> GT
%token <string> LT
%token <string> GTE
%token <string> LTE
%token <string> NL
%token <string> NIL
%token <string> CALL
%token <character> CHAR
%token <string> END
%token <string> EACH
%token <string> EACH_INLINE
%token <string> INCLUDE
%token <string> VARIABLE
%token <string> ARROW
%token <string> WORD
%token <string> IF
%token <string> SET
%token <string> ELSE

%type <number> type
%type <string> charseq
%type <cv> string
%type <string> callfunc
%type <string> varstr
%type <string> varname
%type <string> callstatement
%type <cv> variable
%type <keyvec> vector
%type <cv> typedvalue
%type <cv> term
%type <cmp> operator
%%

files:
	files file
	|
	file
;


file:
	eof 
	|
	tokens eof
;

tokens:	tokens token
	|
	token
	;	

eof:	MY_EOF {
	    code_stack_t *stack;
	    
	    fflush(stdout);
	    stack = (code_stack_t *)_YA->ya_code_stack;
	    db2txt_debug("EOF reached. File = '%s'; Stack = %s (%p)\n", 
			 _YA->ya_file_stack->name ? _YA->ya_file_stack->name : "__buffer__",
			 db2txt_code_string(stack) ,stack);
	    if (stack == NULL) /* End of main file */
		YYACCEPT;

	    switch(stack->codetype) {
	    case EACH:
		clicon_db2txterror(_YA, "@%s at line %d unterminated", 
				   db2txt_code_string(stack),
				   stack->startline);
		YYERROR;
		break;
	    case EACH_INLINE:
		end_each(_YA);
		break;
	    case INCLUDE:
		yyclearin;
		pop_code(_YA);
		db2txt_popfile(_YA);
		break;
	    case IF:
	    case ELSE:
		clicon_db2txterror(_YA, "@if at line %d unterminated", 
				   stack->startline);
		YYERROR;
		break;
	    }
	    
 	} 
;

token:  
	CHAR {
	    char str[2];
	    sprintf(str, "%c", clicon_db2txtlval.character);
	    if (db2txt_out(_YA, str) != 0)
		YYERROR;
	}
	|
	NL {
	    if (db2txt_out(_YA, "\n") != 0)
		YYERROR;
	}
	|
	variable {	
	    char *str;
	    str = cv2str_dup($1);
	    cv_free($1);
	    if (str == NULL) {
		clicon_db2txterror(_YA, "malloc failed");
		YYERROR;
	    }
	    if ((db2txt_out(_YA, str)) != 0) {
		free(str);
		YYERROR;
	    }
	    free(str);
	}
	| 
	code
;

operator:
	EQ { $$ = cmp_eq; }
	|
	NEQ { $$ = cmp_neq; }
	|
	GT { $$ = cmp_gt; }
	|
	LT { $$ = cmp_lt; }
	|
	GTE { $$ = cmp_gte; }
	|
	LTE { $$ = cmp_lte; }
;

eat_newline:
	/* empty */
	|
	NL
;

code:	
	IF ifstatement eat_newline
	|
	SET setstatement eat_newline
	|
	elsestatement eat_newline
	|
	endstatement eat_newline
	|
	INCLUDE filestatement eat_newline
	|
	EACH eachstatement eat_newline
	|
	callstatement eat_newline {
	    if ($1 != NULL) {
		if (db2txt_out(_YA, $1) != 0)  {
		    free($1);
		    YYERROR;
		}
		free($1);
	    }		
	}
;


term:
	variable {
	    $$ = $1;
	}
	|
	typedvalue {
	    $$ = $1;
	}
	|
	NIL {
	    $$ = get_nullvar(_YA);
	    if ($$ == NULL)
		YYERROR;
	}
	;

ifstatement:	
	'(' term ')'  {
	    push_code(_YA, cmp_true($2), IF);
	    cv_free($2);
	}
	|
	'(' term '?' string ')'  {
	    if (cmp_true($2))
		db2txt_out(_YA, cv_string_get($4));
	    cv_free($2);
	    cv_free($4);
	}
	|
	'(' term '?' string ':' string ')'  {
	    if (cmp_true($2))
		db2txt_out(_YA, cv_string_get($4));
	    else
		db2txt_out(_YA, cv_string_get($6));
	    cv_free($2);
	    cv_free($4);
	    cv_free($6);
	}
	|
	'(' term operator term ')'  {
	    push_code(_YA, $3($2, $4), IF);
	    cv_free($2);
	    cv_free($4);
	}
	|
	'(' term operator term '?' string ')'  {
	    if ($3($2, $4))
		db2txt_out(_YA, cv_string_get($6));
	    cv_free($2);
	    cv_free($4);
	    cv_free($6);
	}
	|
	'(' term operator term '?' string ':' string ')'  {
	    if ($3($2, $4))
		db2txt_out(_YA, cv_string_get($6));
	    else
		db2txt_out(_YA, cv_string_get($8));
	    cv_free($2);
	    cv_free($4);
	    cv_free($6);
	    cv_free($8);
	}
	;

elsestatement:
	ELSE {
	    code_stack_t *cs, *parent;
	    
	    if ((cs = (code_stack_t *)_YA->ya_code_stack) == NULL) {
		if (cs->codetype == ELSE) {
		    clicon_db2txterror(_YA, "multiple @else");
		    YYERROR;
		}
		if (cs->codetype != IF) {
		    clicon_db2txterror(_YA, "@else without @if");
		    YYERROR;
		}
	    }
	    parent = NEXTQ(code_stack_t *, cs);
	    if (parent != NULL && parent != cs && parent->processing)
		cs->processing = ! cs->processing;
	    cs->codetype = ELSE;
	}
;

endstatement:
 	END { 
	    code_stack_t *cs;
	    
	    yyclearin;
	    
	    if ((cs = (code_stack_t *)_YA->ya_code_stack) == NULL ||
		(cs->codetype != IF && cs->codetype != ELSE &&
		 cs->codetype != EACH && cs->codetype != EACH_INLINE)) {
		clicon_db2txterror(_YA, "unexpected @end");
		YYERROR;
	    }
		
	    if (cs->codetype == EACH || cs->codetype == EACH_INLINE)
		end_each(_YA);
	    else
		pop_code(_YA); 

	}
;

setstatement:
	'(' '$' VARIABLE ',' term ')' {
	    if (cv_name_set($5, $3) == NULL) {
		clicon_db2txterror(NULL, "malloc failed");
		free($3);
		cv_free($5);
		YYERROR;
	    }
	    free($3);
	    if (cvec_add_cv(_YA->ya_vars, $5) == NULL) {
		clicon_db2txterror(NULL, "malloc failed");
		cv_free($5);
		YYERROR;
	    }		
	    cv_free($5);
	}
	|
	'(' '$' VARIABLE ',' callstatement ')' {
	    if ($5 != NULL) {
		cg_var *cv;
		if ((cv = cvec_add(_YA->ya_vars, CGV_STRING)) == NULL) {
		    clicon_db2txterror(NULL, "malloc failed");
		    free($3);
		    free($5);
		    YYERROR;
		}
		if (cv_name_set(cv, $3) == NULL || cv_string_set(cv, $5) == NULL) {
		    free($3);
		    free($5);
		    cvec_del(_YA->ya_vars, cv);
		    cv_free(cv);
		    clicon_db2txterror(NULL, "cv_string_set failed");
		    YYERROR;
		}
		free($5);
	    }
	    free($3);
	}
;

eachstatement:
	'(' vector ',' '$' VARIABLE ')'  
	{
	    code_stack_t *parent, *cs;

	    yyclearin;

	    parent = (code_stack_t *)_YA->ya_code_stack;
	    cs = push_code(_YA, FALSE, EACH);
	    cs->keyvec = $2;
	    cs->loopvar = $5;
	    if (parent && parent->processing == FALSE)
		$2->len = 0; /* Set vector length to 0 to avoid looping */
	    if ($2->len > 0)   /* Start registering tokens to re-iterate */

		cs->append_loopbuf = TRUE;
	}
|
	'(' vector ',' '$' VARIABLE ',' string ')' eat_newline {
	    code_stack_t *cs;
	    
	    yyclearin;
	    cs = (code_stack_t *)_YA->ya_code_stack;
	    if (!cs || cs->processing == TRUE) {  
		cs = push_code(_YA, FALSE, EACH_INLINE);
		cs->keyvec = $2;
		cs->loopvar = $5;
		cs->loopbuf = strdup(cv_string_get($7));
		cs->append_loopbuf = TRUE; /* Setup state for end_each() */
		end_each(_YA);
		cv_free($7);
	    }
	    else {
		free($5);
		free($2->vec);
		free($2);
		cv_free($7);
	    }
	}
;

filestatement:
	'(' string ')' eat_newline  {
	    code_stack_t *stack;
	    
	    /* Only bother if in processing mode */
	    stack = (code_stack_t *)_YA->ya_code_stack;
	    if (!stack || stack->processing == TRUE) {  
		file_stack_t *fs;

		/* Avoid recursive includes */
		if ((fs = _YA->ya_file_stack) != NULL) {
		    char **p1 = clicon_realpath(NULL, cv_string_get($2), "db2txt-realpath");
		    do { 
			if (fs->name) {
			    char **p2 = clicon_realpath(NULL, fs->name, "db2txt-realpath");
			    if (p1 == NULL || p2 == NULL) {
				clicon_db2txterror(_YA, "Failed to resolve path'");
				unchunk_group("db2txt-realpath");
				cv_free($2);
				YYERROR;
			    }
			    if (strcmp(p1[0], p2[0]) == 0) {
				clicon_db2txterror(_YA,"recursive @inlcude of '%s'",
						   cv_string_get($2));
				unchunk_group("db2txt-realpath");
				cv_free($2);
			    YYERROR;
			    }
			}
			fs = NEXTQ(file_stack_t *, fs);
		    } while (fs != _YA->ya_file_stack);
		    unchunk_group("db2txt-realpath");
		}
		
		yyclearin;

		if (push_code(_YA, TRUE, INCLUDE) == NULL) {
		    cv_free($2);
		    clicon_db2txterror(_YA, "malloc failed");
		    YYERROR;
		}
		if ((fs = db2txt_file_new(cv_string_get($2))) == NULL) {
		    clicon_db2txterror(_YA," failed to read '%s'", cv_string_get($2));
		    cv_free($2);
		    YYERROR;
		}
		db2txt_pushfile(_YA, fs);
	    }
	    cv_free($2);
	}
;

callstatement:
	'$' callfunc '(' typedvalue ')' {
	    yyclearin;
	    $$ = run_callback(_YA, $2, $4);
	    free($2);
	    cv_free($4);
	}
	|
	
	'$' callfunc '(' variable ')' {
	    yyclearin;
	    $$ = run_callback(_YA, $2, $4);
	    free($2);
	    cv_free($4);
	}
	|
	'$' callfunc '(' ')' {
	    yyclearin;
	    $$ = run_callback(_YA, $2, NULL);
	    free($2);
	}
;

callfunc:
	VARIABLE ':' ':' VARIABLE {
	    int len = strlen($1) + 2 + strlen($4) + 1;
	    if (($$ = malloc(len)) == NULL) {
		clicon_db2txterror(_YA, "malloc failed");
		free($1);
		free($4);
		YYERROR;
	    }
	    snprintf($$, len, "%s::%s", $1, $4);
	    free($1);
	    free($4);
	}
	|
	VARIABLE {
	    $$ = $1;
	}
;

type: WORD {
    $$ = cv_str2type($1);
    if ($$ == CGV_ERR) {
	clicon_db2txterror(_YA, "Invalid type");
	free($1);
	YYERROR;
    }
    free($1);
}
;
    
typedvalue:
	string {
	    $$ = $1;
	}
	|
	'(' type ')' string {
	    if ($2 != CGV_STRING) {
		clicon_db2txterror(_YA, "Cannot parse value");
		cv_free($4);
		YYERROR;
	    }
	    $$ = $4;
	}
	|
	'(' type ')' WORD {   /* XXX fix me */
	    $$ = parse_val($2, $4);
	    if ($$ == NULL) {
		clicon_db2txterror(_YA, "Cannot parse value");
		free($4);
		YYERROR;
	    }
	    free($4);
	}
	;


string: 
	'"' charseq '"' {
	    if (($$ = cv_new(CGV_STRING)) == NULL) {
		clicon_db2txterror(_YA, "malloc failed");
		free($2);
		YYERROR;
	    }
	    if (cv_string_set($$, $2) == NULL) {
		free($2);
		cv_free($$);
		clicon_db2txterror(_YA, "malloc failed");
		YYERROR;
	    }
	    free($2);
	}
	;
	
charseq:
	CHAR {
	    if (($$ = malloc(2)) == NULL) {
		clicon_db2txterror(_YA, "malloc failed");
		YYERROR;
	    }
	    sprintf($$, "%c", $1);
	}
	|
	charseq CHAR {
	    int len = strlen($1);
	    if (($$ = realloc($1, len+2)) == NULL) {
		clicon_db2txterror(_YA, "malloc failed");
		YYERROR;
	    }
	    sprintf($$+len, "%c", $2); 
	}
	|
	charseq variable {
	    char *v;
	    int len = strlen($1);

	    if ((v = cv2str_dup($2)) != NULL) {
		if (($$ = realloc($1, len+strlen(v)+1)) == NULL) {
		    clicon_db2txterror(_YA, "malloc failed");
		    YYERROR;
		}
		sprintf($$+len, "%s", v);
		free(v);
	    }
	    cv_free($2);
	}
;

varstr:
	VARIABLE ARROW VARIABLE {
	    int len = strlen($1) + 2 + strlen($3) + 1;
	    if (($$ = malloc(len)) == NULL) {
		clicon_db2txterror(_YA, "malloc failed");
		free($1);
		free($3);
		YYERROR;
	    }
	    snprintf($$, len, "%s->%s", $1, $3);
	    free($1);
	    free($3);
	}
	|
	VARIABLE {
	    $$ = $1;
	}
;

varname:
	'{' varstr '}' {
	    $$ = $2;
	}
	|
	varstr {
	    $$ = $1;
	}
;
		

variable:
 	'$' varname {
	    char *val;
	    cg_var *cv;

	    if (_YA->ya_vars && (cv = cvec_find(_YA->ya_vars, $2)) != NULL) 
		$$ = cv_dup(cv);
	    else if ((val = clicon_option_str(_YA->ya_handle, $2)) != NULL) {
		if (($$ = cv_new(CGV_STRING)) == NULL)
		    clicon_db2txterror(_YA, "cv_new failed");
		else if (cv_string_set($$, val) == NULL) {
		    clicon_db2txterror(_YA, "cv_string_set failed");
		    cv_free($$);
		    $$ = NULL;
		}
	    }		
	    else
		$$ = get_dbvar(_YA, $2);
	    free($2);
	    if ($$ == NULL)
		YYERROR;
	}
;

vector:
	'$' '{' VARIABLE '}' '[' ']' {
	    if (($$ = get_vector(_YA, $3)) == NULL)
		YYERROR;
	    free($3);
	}
	|
	'$' VARIABLE '[' ']' {
	    if (($$ = get_vector(_YA, $2)) == NULL)
		YYERROR;
	    free($2);
	}

;


%%
