/*
 *
  Copyright (C) 2009-2015 Olof Hagsand and Benny Holmgren

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
 * Translation between database specs
 *     dbspec_key                    yang_spec                   CLIgen parse_tree
 *  +-------------+    key2yang    +-------------+   yang2cli    +-------------+
 *  |  keyspec    | -------------> |             | ------------> | cli         |
 *  |  A[].B !$a  |    yang2key    | list{key A;}|               | syntax      |
 *  +-------------+ <------------  +-------------+               +-------------+
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


/*=====================================================================
 * YANG generate CLI
 *=====================================================================*/
#if 0 /* examples/ntp */
 ntp("Network Time Protocol"),cli_set("ntp");{ 
     logging("Configure NTP message logging"),cli_set("ntp.logging");{ 
	 status (<status:bool>),cli_set("ntp.logging $status:bool");
     } 
     server("Configure NTP Server") (<ipv4addr:ipv4addr>("IPv4 address of peer")),cli_set("ntp.server[] $!ipv4addr:ipv4addr");
 }
#endif
#if 0 /* examples/datamodel */

WITH COMPLETION:
 a (<x:number>|<x:number expand_dbvar_auto("candidate a[] $!x")>),cli_set("a[] $!x");{
     b,cli_set("a[].b $!x");{ 
	 y (<y:string>|<y:string expand_dbvar_auto("candidate a[].b $!x $y")>),cli_set("a[].b $!x $y");
     } 
     z (<z:string>|<z:string expand_dbvar_auto("candidate a[] $!x $z")>),cli_set("a[] $!x $z");
 }

#endif

#ifndef HAVE_CLIGEN_MAX2STR /* XXX cligen 3.6 feature */

/*! Print max value of a CLIgen variable type as string
 * @param[in]   type  CLIgen variable type
 * @param[out]  str   Max value printed in this string
 * @param[in]   size  Length of 'str'
 * @retval len  How many bytes printed
 * @see cvtype_max2str_dup
 * You can use str=NULL to get the expected length.
 * The number of (potentially if str=NULL) written bytes is returned.
 */
static int
cvtype_max2str(enum cv_type type, char *str, size_t size)
{
    int  len = 0;

    switch (type){
    case CGV_INT8:
	len = snprintf(str, size, "%" PRId8, SCHAR_MAX);
	break;
    case CGV_INT16:
	len = snprintf(str, size, "%" PRId16, SHRT_MAX);
	break;
    case CGV_INT32:
	len = snprintf(str, size, "%" PRId32, INT_MAX);
	break;
    case CGV_INT64:
	len = snprintf(str, size, "%" PRId64, LLONG_MAX);
	break;
    case CGV_UINT8:
	len = snprintf(str, size, "%" PRIu8, UCHAR_MAX);
	break;
    case CGV_UINT16:
	len = snprintf(str, size, "%" PRIu16, USHRT_MAX);
	break;
    case CGV_UINT32:
	len = snprintf(str, size, "%" PRIu32, UINT_MAX);
	break;
    case CGV_UINT64:
	len = snprintf(str, size, "%" PRIu64, ULLONG_MAX);
	break;
    case CGV_DEC64:
	len = snprintf(str, size, "%" PRId64 ".0", LLONG_MAX);
	break;
    case CGV_BOOL:
	len = snprintf(str, size, "true");
	break;
    default:
	break;
    }
    return len;
}

/*! Print max value of a CLIgen variable type as string
 *
 * The string should be freed after use.
 * @param[in]   type  CLIgen variable type
 * @retval      str   Malloced string containing value. Should be freed after use.
 * @see cvtype_max2str
 */
static char *
cvtype_max2str_dup(enum cv_type type)
{
    int   len;
    char *str;

    if ((len = cvtype_max2str(type, NULL, 0)) < 0)
	return NULL;
    if ((str = (char *)malloc (len+1)) == NULL)
	return NULL;
    memset (str, '\0', len+1);
    if ((cvtype_max2str(type, str, len+1)) < 0){
	free(str);
	return NULL;
    }
    return str;
}
#endif /* HAVE_CLIGEN_MAX2STR */

static int yang2cli_stmt(clicon_handle h, yang_stmt    *ys, 
			 cbuf         *cbuf,    
			 enum genmodel_type gt,
			 int           level);

/*
 * Check for completion (of already existent values), ranges (eg range[min:max]) and
 * patterns, (eg regexp:"[0.9]*").
 */
static int
yang2cli_var_sub(clicon_handle h,
		 yang_stmt    *ys, 
		 cbuf         *cbuf,    
		 char         *description,
		 enum cv_type  cvtype,
		 yang_stmt    *ytype,  /* resolved type */
		 int           options,
		 cg_var       *mincv,
		 cg_var       *maxcv,
		 char         *pattern,
		 uint8_t       fraction_digits
    )
{
    int           retval = -1;
    char         *type;
    char         *r;
    yang_stmt    *yi = NULL;
    int           i = 0;
    char         *cvtypestr;
    int           completion;

    /* enumeration already gives completion */
    if (cvtype == CGV_VOID){
	retval = 0;
	goto done;
    }
    type = ytype?ytype->ys_argument:NULL;
    if (type)
	completion = clicon_cli_genmodel_completion(h) &&
	    strcmp(type, "enumeration") != 0 && 
	    strcmp(type, "bits") != 0;
    else
	completion = clicon_cli_genmodel_completion(h);

    if (completion)
	cprintf(cbuf, "(");
    cvtypestr = cv_type2str(cvtype);
    cprintf(cbuf, "<%s:%s", ys->ys_argument, cvtypestr);
#if 0
    if (type && (strcmp(type, "identityref") == 0)){
	yang_stmt      *ybase;
	if ((ybase = yang_find((yang_node*)ytype, Y_BASE, NULL)) != NULL){
	    cprintf(cbuf, " choice:"); 
	    i = 0;
	    /* for every found identity derived from base-type , do: */
	    {
		if (yi->ys_keyword != Y_ENUM && yi->ys_keyword != Y_BIT)
		    continue;
		if (i)
		    cprintf(cbuf, "|"); 
		cprintf(cbuf, "%s", yi->ys_argument); 
		i++;
	    }
	}

    }
#endif
    if (type && (strcmp(type, "enumeration") == 0 || strcmp(type, "bits") == 0)){
	cprintf(cbuf, " choice:"); 
	i = 0;
	while ((yi = yn_each((yang_node*)ytype, yi)) != NULL){
	    if (yi->ys_keyword != Y_ENUM && yi->ys_keyword != Y_BIT)
		continue;
	    if (i)
		cprintf(cbuf, "|"); 
	    cprintf(cbuf, "%s", yi->ys_argument); 
	    i++;
	}
    }
    if (options & YANG_OPTIONS_FRACTION_DIGITS)
	cprintf(cbuf, " fraction-digits:%u", fraction_digits);
    if (options & (YANG_OPTIONS_RANGE|YANG_OPTIONS_LENGTH)){
	cprintf(cbuf, " %s[", (options&YANG_OPTIONS_RANGE)?"range":"length");
	if (mincv){
	    if ((r = cv2str_dup(mincv)) == NULL){
		clicon_err(OE_UNIX, errno, "cv2str_dup");
		goto done;
	    }
	    cprintf(cbuf, "%s:", r);
	    free(r);
	}
	if (maxcv != NULL){
	    if ((r = cv2str_dup(maxcv)) == NULL){
		clicon_err(OE_UNIX, errno, "cv2str_dup");
		goto done;
	    }

	}
	else{ /* Cligen does not have 'max' keyword in range so need to find actual
		 max value of type if yang range expression is 0..max */
	    if ((r = cvtype_max2str_dup(cvtype)) == NULL){
		clicon_err(OE_UNIX, errno, "cvtype_max2str");
		goto done;
	    }
	}
	cprintf(cbuf, "%s]", r);
	free(r);
    }
    if (options & YANG_OPTIONS_PATTERN)
	cprintf(cbuf, " regexp:\"%s\"", pattern);

    cprintf(cbuf, ">");
    if (description)
	cprintf(cbuf, "(\"%s\")", description);
    if (completion){
	cprintf(cbuf, "|<%s:%s expand_dbvar_auto(\"candidate %s\")>",
		ys->ys_argument, 
		cv_type2str(cvtype),
		yang_dbkey_get(ys));
	if (description)
	    cprintf(cbuf, "(\"%s\")", description);
	cprintf(cbuf, ")");
    }
    retval = 0;
  done:
    return retval;
}

/*! Translate a yang leaf to cligen variable
 * Make a type lookup and complete a cligen variable expression such as <a:string>.
 * One complication is yang union, that needs a recursion since it consists of sub-types.
 * eg type union{ type int32; type string } --> (<x:int32>| <x:string>)
 */
static int
yang2cli_var(clicon_handle h,
	     yang_stmt    *ys, 
	     cbuf         *cbuf,    
	     char         *description)
{
    int retval = -1;
    char         *type;  /* orig type */
    yang_stmt    *yrestype; /* resolved type */
    char         *restype; /* resolved type */
    cg_var       *mincv = NULL; 
    cg_var       *maxcv = NULL;
    char         *pattern = NULL;
    yang_stmt    *yt = NULL;
    yang_stmt    *yrt;
    uint8_t       fraction_digits = 0;
    enum cv_type  cvtype;
    int           options = 0;
    int           i;

    if (yang_type_get(ys, &type, &yrestype, 
		      &options, &mincv, &maxcv, &pattern, &fraction_digits) < 0)
	goto done;
    restype = yrestype?yrestype->ys_argument:NULL;
    if (clicon_type2cv(type, restype, &cvtype) < 0)
	goto done;
    /* Note restype can be NULL here for example with unresolved hardcoded uuid */
    if (restype && strcmp(restype, "union") == 0){ 
	/* Union: loop over resolved type's sub-types */
	cprintf(cbuf, "(");
	yt = NULL;
	i = 0;
	while ((yt = yn_each((yang_node*)yrestype, yt)) != NULL){
	    if (yt->ys_keyword != Y_TYPE)
		continue;
	    if (i++)
		cprintf(cbuf, "|");
	    if (yang_type_resolve(ys, yt, &yrt, 
				  &options, &mincv, &maxcv, &pattern, &fraction_digits) < 0)
		goto done;
	    restype = yrt?yrt->ys_argument:NULL;
	    if (clicon_type2cv(type, restype, &cvtype) < 0)
		goto done;
	    if ((retval = yang2cli_var_sub(h, ys, cbuf, description, cvtype, yrt,
					   options, mincv, maxcv, pattern, fraction_digits)) < 0)

		goto done;

	}
	cprintf(cbuf, ")");
    }
    else
	if ((retval = yang2cli_var_sub(h, ys, cbuf, description, cvtype, yrestype,
				    options, mincv, maxcv, pattern, fraction_digits)) < 0)
	    goto done;

    retval = 0;
  done:
    return retval;
}

/*!
 * @param[in]  h         Clicon handle
 * @param[in]  callback  If set, include a "; cli_set()" callback, otherwise not.
 */
static int
yang2cli_leaf(clicon_handle h, 
	      yang_stmt    *ys, 
	      cbuf         *cbuf,
	      enum genmodel_type gt,
	      int           level,
	      int           callback)
{
    yang_stmt    *yd;  /* description */
    int           retval = -1;
    char         *keyspec;
    char         *description = NULL;

    /* description */
    if ((yd = yang_find((yang_node*)ys, Y_DESCRIPTION, NULL)) != NULL)
	description = yd->ys_argument;
    cprintf(cbuf, "%*s", level*3, "");
    if (gt == GT_VARS|| gt == GT_ALL){
	cprintf(cbuf, "%s", ys->ys_argument);
	if (yd != NULL)
	    cprintf(cbuf, "(\"%s\")", yd->ys_argument);
	cprintf(cbuf, " ");
	yang2cli_var(h, ys, cbuf, description);
    }
    else
	yang2cli_var(h, ys, cbuf, description);
    if (callback){
	if ((keyspec = yang_dbkey_get(ys)) != NULL)
	    cprintf(cbuf, ",cli_set(\"%s\")", keyspec);
	cprintf(cbuf, ";\n");
    }

    retval = 0;
//  done:
    return retval;
}


static int
yang2cli_container(clicon_handle h, 
		   yang_stmt    *ys, 
		   cbuf         *cbuf,
		   enum genmodel_type gt,
		   int           level)
{
    yang_stmt    *yc;
    yang_stmt    *yd;
    char         *keyspec;
    int           i;
    int           retval = -1;

    cprintf(cbuf, "%*s%s", level*3, "", ys->ys_argument);
    if ((yd = yang_find((yang_node*)ys, Y_DESCRIPTION, NULL)) != NULL)
	cprintf(cbuf, "(\"%s\")", yd->ys_argument);
    if ((keyspec = yang_dbkey_get(ys)) != NULL)
	cprintf(cbuf, ",cli_set(\"%s\");", keyspec);
   cprintf(cbuf, "{\n");
    for (i=0; i<ys->ys_len; i++)
	if ((yc = ys->ys_stmt[i]) != NULL)
	    if (yang2cli_stmt(h, yc, cbuf, gt, level+1) < 0)
		goto done;
    cprintf(cbuf, "%*s}\n", level*3, "");
    retval = 0;
  done:
    return retval;
}

static int
yang2cli_list(clicon_handle h, 
	      yang_stmt    *ys, 
	      cbuf         *cbuf,
	      enum genmodel_type gt,
	      int           level)
{
    yang_stmt    *yc;
    yang_stmt    *yd;
    yang_stmt    *ykey;
    yang_stmt    *yleaf;
    int           i;
    cg_var       *cvi;
    char         *keyname;
    cvec         *cvk = NULL; /* vector of index keys */
    int           retval = -1;

    cprintf(cbuf, "%*s%s", level*3, "", ys->ys_argument);
    if ((yd = yang_find((yang_node*)ys, Y_DESCRIPTION, NULL)) != NULL)
	cprintf(cbuf, "(\"%s\")", yd->ys_argument);
    /* Loop over all key variables */
    if ((ykey = yang_find((yang_node*)ys, Y_KEY, NULL)) == NULL){
	clicon_err(OE_XML, 0, "List statement \"%s\" has no key", ys->ys_argument);
	goto done;
    }
    if ((cvk = yang_arg2cvec(ykey, " ")) == NULL)
	goto done;
    cvi = NULL;
    while ((cvi = cvec_each(cvk, cvi)) != NULL) {
	keyname = cv_string_get(cvi);
	if ((yleaf = yang_find((yang_node*)ys, Y_LEAF, keyname)) == NULL){
	    clicon_err(OE_XML, 0, "List statement \"%s\" has no key leaf \"%s\"", 
		       ys->ys_argument, keyname);
	    goto done;
	}
	/* Print key variable now, and skip it in loop below 
	   Note, only print callbcak on last statement
	 */
	if (yang2cli_leaf(h, yleaf, cbuf, gt==GT_VARS?GT_NONE:gt, level+1, 
			  cvec_next(cvk, cvi)?0:1) < 0)
	    goto done;
    }

    cprintf(cbuf, "{\n");
    for (i=0; i<ys->ys_len; i++)
	if ((yc = ys->ys_stmt[i]) != NULL){
	    /*  cvk is a cvec of strings containing variable names
		yc is a leaf that may match one of the values of cvk.
	     */
	    cvi = NULL;
	    while ((cvi = cvec_each(cvk, cvi)) != NULL) {
		keyname = cv_string_get(cvi);
		if (strcmp(keyname, yc->ys_argument) == 0)
		    break;
	    }
	    if (cvi != NULL)
		continue;
	    if (yang2cli_stmt(h, yc, cbuf, gt, level+1) < 0)
		goto done;
	}
    cprintf(cbuf, "%*s}\n", level*3, "");
    retval = 0;
  done:
    if (cvk)
	cvec_free(cvk);
    return retval;
}
/*

  container food {
       choice snack {
           case sports-arena {
               leaf pretzel {
                   type empty;
               }
               leaf beer {
                   type empty;
               }
           }
           case late-night {
               leaf chocolate {
                   type enumeration {
                       enum dark;
                       enum milk;
                       enum first-available;
                   }
               }
           }
       }
    }

set food {
    snack{
     sports-arena{
       pretzel;
       beer;
     }
     late-night{
      chocolate;
    }
  }
}
 */
static int
yang2cli_choice(clicon_handle h, 
		yang_stmt    *ys, 
		cbuf         *cbuf,
		enum genmodel_type gt,
		int           level)
{
    int           retval = -1;
    yang_stmt    *yc;
    int           i;

    cprintf(cbuf, "%*s%s{\n", level*3, "", ys->ys_argument);
    for (i=0; i<ys->ys_len; i++)
	if ((yc = ys->ys_stmt[i]) != NULL){
	    switch (yc->ys_keyword){
	    case Y_CASE:
		cprintf(cbuf, "%*s%s{\n", 3*(level+1), "", yc->ys_argument);
		if (yang2cli_stmt(h, yc, cbuf, gt, level+2) < 0)
		    goto done;
		cprintf(cbuf, "}");
		break;
	    case Y_CONTAINER:
	    case Y_LEAF:
	    case Y_LEAF_LIST:
	    case Y_LIST:
	    default:
		if (yang2cli_stmt(h, yc, cbuf, gt, level+1) < 0)
		    goto done;
		break;
	    }
	}
    cprintf(cbuf, "%*s}\n", level*3, "");
    retval = 0;
  done:
    return retval;
}


/*! Translate yang-stmt to CLIgen syntax.
 */
static int
yang2cli_stmt(clicon_handle h, 
	      yang_stmt    *ys, 
	      cbuf         *cbuf,
	      enum genmodel_type gt,
	      int           level /* indentation level for pretty-print */
    )
{
    yang_stmt    *yc;
    int           retval = -1;
    int           i;

    if (yang_config(ys)){
	switch (ys->ys_keyword){
	case Y_GROUPING:
	case Y_RPC:
	case Y_AUGMENT:
	    return 0;
	    break;
	case Y_CONTAINER:
	    if (yang2cli_container(h, ys, cbuf, gt, level) < 0)
		goto done;
	    break;
	case Y_LIST:
	    if (yang2cli_list(h, ys, cbuf, gt, level) < 0)
		goto done;
	    break;
	case Y_CHOICE:
	    if (yang2cli_choice(h, ys, cbuf, gt, level) < 0)
		goto done;
	    break;
	case Y_LEAF_LIST:
	case Y_LEAF:
	    if (yang2cli_leaf(h, ys, cbuf, gt, level, 1) < 0)
		goto done;
	    break;
	default:
	    for (i=0; i<ys->ys_len; i++)
		if ((yc = ys->ys_stmt[i]) != NULL)
		    if (yang2cli_stmt(h, yc, cbuf, gt, level+1) < 0)
			goto done;
	    break;
	}
    }

    retval = 0;
  done:
    return retval;

}

/*! Translate from a yang specification into a CLIgen syntax.
 *
 * Print a CLIgen syntax to cbuf string, then parse it.
 * @param gt - how to generate CLI: 
 *             VARS: generate keywords for regular vars only not index
 *             ALL:  generate keywords for all variables including index
 */
int
yang2cli(clicon_handle h, 
	 yang_spec *yspec, 
	 parse_tree *ptnew, 
	 enum genmodel_type gt)
{
    cbuf           *cbuf;
    int             i;
    int             retval = -1;
    yang_stmt      *ymod = NULL;
    cvec           *globals;       /* global variables from syntax */

    if ((cbuf = cbuf_new()) == NULL){
	clicon_err(OE_XML, errno, "%s: cbuf_new", __FUNCTION__);
	goto done;
    }
    /* Traverse YANG specification: loop through statements */
    for (i=0; i<yspec->yp_len; i++)
	if ((ymod = yspec->yp_stmt[i]) != NULL){
	    if (yang2cli_stmt(h, ymod, cbuf, gt, 0) < 0)
		goto done;
	}
    clicon_debug(1, "%s: buf\n%s\n", __FUNCTION__, cbuf_get(cbuf));
    /* Parse the buffer using cligen parser. XXX why this?*/
    if ((globals = cvec_new(0)) == NULL)
	goto done;
    /* load cli syntax */
    if (cligen_parse_str(cli_cligen(h), cbuf_get(cbuf), 
			 "yang2cli", ptnew, globals) < 0)
	goto done;

    cvec_free(globals);
    /* handle=NULL for global namespace, this means expand callbacks must be in
       CLICON namespace, not in a cli frontend plugin. */
    if (cligen_expand_str2fn(*ptnew, expand_str2fn, NULL) < 0)     
	goto done;
    retval = 0;
  done:
    cbuf_free(cbuf);
    return retval;
}
