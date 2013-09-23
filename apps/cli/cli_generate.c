/*
 *  CVS Version: $Id: cli_generate.c,v 1.15 2013/09/18 19:20:50 olof Exp $
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

 *
 *     db_spec                      parse_tree                    parse_tree
 *  +-------------+ dbspec_key2cli +-------------+ dbspec2cli    +-------------+
 *  |  dbspec     | -------------> | dbclispec   | ------------> | cli         |
 *  |  A[].B !$a  | dbspec_cli2key | A <!a>{ B;} |               | syntax      |
 *  +-------------+ <------------  +-------------+               +-------------+
 *        ^                               ^
 *        |db_spec_parse_file             | dbclispec_parse
 *        |                               |
 *      <file>                          <file>
 */
#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/param.h>


/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include <clicon/clicon.h>

#include "clicon_cli_api.h"
#include "cli_plugin.h"
#include "cli_generate.h"


/*
 * dbspec2cli_co
 * Translate from a database specification (cli form) into a cligen syntax.
 * co sub-fn of dbspec2cli.
 * Take a cli spec tree and transform it into a cligen syntax tree.
 * More specifically, for every non-index variable, make a 'merge' rule.
 */


/*
 * Find variable in pt with a certain name 
 * very similar to co_find_one(), only Variable-check differs.
 */
static cg_obj *
find_index_variable(parse_tree *pt, char *name)
{
    int i;
    cg_obj *co = NULL;

    for (i=0; i<pt->pt_len; i++){
	co = pt->pt_vec[i];
	if (co && 
	    co->co_type == CO_VARIABLE && 
	    strcmp(name, co->co_command)==0)
	    break; /* found */
	co = NULL; /* only inde variable should be cov */
    }
    return co;
}


int
print_symbol(xf_t *xf, cg_obj *co)
{
    xprintf(xf, " %s", co->co_command);
    if (co->co_help)
	xprintf(xf, "(\"%s\")", co->co_help);
    return 0;
}

/* forward */
static int
dbspec2cli_co(clicon_handle h, xf_t *xf, cg_obj *co, enum genmodel_type gt);

static int
dbspec2cli_co_cmd(clicon_handle h, xf_t *xf, cg_obj *co, enum genmodel_type gt)
{
    int         retval = -1;
    parse_tree *pt;
    cg_obj     *cov;
    cg_obj     *co1;
    int         i;
    int         subs; /* yes, there are children */
    char       *ivar;
    
    pt = &co->co_pt;
    print_symbol(xf, co);
    cov = NULL;
    if ((ivar = dbspec_indexvar_get(co)) != NULL){
	/* Find the index variable */
	if ((cov = find_index_variable(pt, ivar)) == NULL){
	    clicon_err(OE_DB, errno, "%s: %s has no indexvariable %s\n", 
		       __FUNCTION__, co->co_command, ivar); 
	    goto done;
	}
	if (gt == GT_ALL) /* Add extra keyword for index variable */
	    print_symbol(xf, cov);
	if (dbspec2cli_co(h, xf, cov, gt) < 0)
	    goto done;
    }
    else{
	if (dbspec_key_get(co))
	    xprintf(xf, ",cli_set(\"%s\")", dbspec_key_get(co));
	xprintf(xf, ";");	    
#if 0 /* obsolete? */
	struct cg_callback *cc;
	for (cc = co->co_callbacks; cc; cc=cc->cc_next){
	    fprintf(stderr, "fn(");
	    if (cc->cc_arg)
		cv_print(stderr, cc->cc_arg);
	    fprintf(stderr, ")");
	}
	fprintf(stderr, "\n");
#endif
    }
    subs = 0; /* Loop thru all other children (skip indexvariable) */
    for (i=0; i<pt->pt_len; i++){ 
	if ((co1 = pt->pt_vec[i]) == NULL)
	    continue;
	if (cov && co1 == cov) /* skip index variable */
	    continue;
	/* Avoid empty brackets: {} by ensuring there are subs */
	if (!subs){
	    xprintf(xf, "{");
	    subs++;
	}
	/* Add extra keyword regular variables */
	if (co1->co_type == CO_VARIABLE &&
	    (gt == GT_ALL || gt == GT_VARS))
	    print_symbol(xf, co1);
	if (dbspec2cli_co(h, xf, co1, gt) < 0)
	    goto done;
    }
    if (subs)
	xprintf(xf, "}");
    retval = 0;
  done:
    return retval;
}

static int 
mycov_print(cg_obj *co, char *cmd, int len, int brief)
{
    char          *cmd2;
    char          *cvstr;

    if (co->co_choice){
	snprintf(cmd, len, "<type:string choice:%s>", co->co_choice);
    }
    else{
	if (brief)
	    snprintf(cmd, len, "<%s>", co->co_command);   
	else{
	    snprintf(cmd, len, "<%s:%s", co->co_command, cv_type2str(co->co_vtype));
	    cmd2 = strdup(cmd);
	    if (co->co_range){
		snprintf(cmd, len, "%s range[%" PRId64 ":%" PRId64 "]", 
			 cmd2, co->co_range_low, co->co_range_high);
		free(cmd2);
		cmd2 = strdup(cmd);
	    }
	    if (co->co_expand_fn_str){
		if (co->co_expand_fn_arg)
		    cvstr = cv2str_dup(co->co_expand_fn_arg);
		else
		    cvstr = NULL;
		snprintf(cmd, len, "%s %s(\"%s\")",  /* XXX: cv2str() */
			 cmd2, co->co_expand_fn_str, cvstr?cvstr:"");
		if (cvstr)
		    free(cvstr);
		free(cmd2);
		cmd2 = strdup(cmd);
	    }
	    if (co->co_regex){
		snprintf(cmd, len, "%s regexp:\"%s\"", 
			 cmd2, co->co_regex);
		free(cmd2);
		cmd2 = strdup(cmd);
	    }
	    snprintf(cmd, len, "%s>", cmd2);
	    free(cmd2);
	}
    }

    return 0;
}


static int
dbspec2cli_co_var(clicon_handle h, xf_t *xf, cg_obj *co, enum genmodel_type gt)
{
    char *s0, *key;
    int retval = -1;

    /* In some cases we should add keyword before variables */	
    xprintf(xf, " (");
    mycov_print(co, 	    /* XXX Warning: this may overflow */
	      xf->xf_buf + xf->xf_len, 
	      xf->xf_maxbuf - xf->xf_len, 0);
    xf->xf_len += strlen(xf->xf_buf+xf->xf_len);
    if (co->co_help)
	xprintf(xf, "(\"%s\")", co->co_help);
    if (clicon_cli_genmodel_completion(h)){
	if ((s0 = strdup(dbspec_key_get(co))) == NULL){
	    clicon_err(OE_DB, errno, "%s: strdup\n", __FUNCTION__); 
	    goto done;
	}
	key = strtok(s0, " ");
	if (key){
	    xprintf(xf, "|");
	    xprintf(xf, "<%s:%s expand_dbvar_auto(\"candidate %s\")>",
		    co->co_command, 
		    cv_type2str(co->co_vtype),
		    dbspec_key_get(co)
		);
	    free(s0); /* includes token */
	    if (co->co_help)
		xprintf(xf, "(\"%s\")", co->co_help);
	}
    }
    xprintf(xf, ")");
    if (dbspec_key_get(co)){
	xprintf(xf, ",cli_set(\"%s\")", dbspec_key_get(co));
	xprintf(xf, ";");
    }
    retval = 0;
  done:
    return retval;
}

/*
 * Miss callbacks here
 */
static int
dbspec2cli_co(clicon_handle h, xf_t *xf, cg_obj *co, enum genmodel_type gt)
{
    int         retval = -1;

    switch (co->co_type){
    case CO_COMMAND: 
	if (dbspec2cli_co_cmd(h, xf, co, gt) < 0)
	    goto done;
	break;
    case CO_VARIABLE: 
	if (dbspec2cli_co_var(h, xf, co, gt) < 0)
	    goto done;
	break;
    default:
	break;
    }
    retval = 0;
  done:
    return retval;
}

/*
 * dbspec2cli
 * Translate from a database specification (cli form) into a cligen syntax.
 * First copy the parse-tree, then modify it.
 * gt - how to generate CLI: 
 *      VARS: generate keywords for regular vars only not index
 *      ALL:  generate keywords for all variables including index
  */
int
dbspec2cli(clicon_handle h, parse_tree *pt, parse_tree *ptnew, enum genmodel_type gt)
{
    int             i;
    cg_obj         *co;
    int             retval = -1;
    xf_t           *xf;
    cvec           *globals;       /* global variables from syntax */

    if ((xf = xf_alloc()) == NULL)
	goto done;
    
    /* Go through parse-tree and print a CLIgen tree. */
    for (i=0; i<pt->pt_len; i++)
	if ((co = pt->pt_vec[i]) != NULL)
	    if (dbspec2cli_co(h, xf, co, gt) < 0)
		goto done;
    /* Parse the buffer using cligen parser. */
    if ((globals = cvec_new(0)) == NULL)
	goto done;
    if (debug)
	fprintf(stderr, "xbuf: %s\n", xf->xf_buf);
    /* load cli syntax */
    if (cligen_parse_str(cli_cligen(h), xf->xf_buf, "dbspec2cli", ptnew, globals) < 0)
	goto done;
    /* Dont check any variables for now (eg name="myspec") */
    cvec_free(globals);
    /* resolve expand function names */
    if (cligen_expand_str2fn(*ptnew, expand_str2fn, NULL) < 0)     
	goto done;

    retval = 0;
  done:
    xf_free(xf);
    return retval;
}

