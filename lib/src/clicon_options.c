/*
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

 *
 * CLICON options
 * See clicon_tutorial appendix for more info
 *
 * CLICON_APPDIR           /usr/local/share/clicon # Application installation directory. 
 * CLICON_CONFIGFILE       $APPDIR/clicon.conf # File containing default option values. 
 * CLICON_DBSPEC_TYPE      PT # PT or KEY
 * CLICON_DBSPEC_FILE      $APPDIR/datamodel.spec # if CLICON_DBSPEC_TYPE is KEY
 * CLICON_YANG_DIR         $APPDIR/yang           # if CLICON_DBSPEC_TYPE is YANG
 * CLICON_YANG_MODULE_MAIN clicon                 # if CLICON_DBSPEC_TYPE is YANG
 * CLICON_CANDIDATE_DB     $APPDIR/db/candidate_db
 * CLICON_RUNNING_DB       $APPDIR/db/running_db
 * CLICON_ARCHIVE_DIR      $APPDIR/archive    # Archive dir (rollback)
 * CLICON_STARTUP_CONFIG   $APPDIR/startup-config # Startup config-file
 * CLICON_SOCK             $APPDIR/clicon.sock # Unix domain socket
 * CLICON_SOCK_GROUP       clicon # Unix group for clicon socket group access
 * CLICON_AUTOCOMMIT       0 # Automatically commit configuration changes (no commit)
 * CLICON_QUIET            # Do not print greetings on stdout. Eg clicon_cli -q
 * CLICON_MASTER_PLUGIN    master.so # Master plugin name. backend and CLI
 * CLICON_BACKEND_DIR      $APPDIR/backend/<group> # Dirs of all backend plugins
 * CLICON_BACKEND_PIDFILE  $APPDIR/clicon.pidfile
 * CLICON_CLI_DIR          $APPDIR/frontend   # Dir of all CLI plugins/ syntax group dirs
 * CLICON_CLI_GROUP        # cli syntax group to start in (clicon_cli)
 * CLICON_CLI_MODE         base # Initial cli syntax mode to start in clicon_cli.
 * CLICON_CLI_GENMODEL     1 # Generate CLIgen syntax from model
 * CLICON_CLI_GENMODEL_TYPE VARS   # NONE|VARS|ALL
 * CLICON_CLI_COMMENT      # # comment char in CLI (default is '#').
 * CLICON_CLI_VARONLY      1 # Dont include keys in cvec in cli vars callbacks
 * CLICON_CLI_GENMODEL_COMPLETION 0 # Generate code for completion
 * CLICON_NETCONF_DIR      $APPDIR/netconf    # Dir of netconf plugins
 *
 * See also appendix in clicon tutorial
 */
#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include "clicon_err.h"
#include "clicon_queue.h"
#include "clicon_hash.h"
#include "clicon_handle.h"
#include "clicon_chunk.h"
#include "clicon_log.h"
#include "clicon_dbspec_key.h"
#include "clicon_yang.h"
#include "clicon_options.h"

/*
 * clicon_option_dump
 * Print registry on file. For debugging.
 */
void
clicon_option_dump(clicon_handle h, int dbglevel)
{
    clicon_hash_t *hash = clicon_options(h);
    int            i;
    char         **keys;
    void          *val;
    size_t         klen;
    size_t         vlen;
    
    if (hash == NULL)
	return;
    keys = hash_keys(hash, &klen);
    if (keys == NULL)
	return;
	
    for(i = 0; i < klen; i++) {
	val = hash_value(hash, keys[i], &vlen);
	if (vlen){
	    if (((char*)val)[vlen-1]=='\0') /* assume string */
		clicon_debug(dbglevel, "%s =\t \"%s\"", keys[i], (char*)val);
	    else
		clicon_debug(dbglevel, "%s =\t 0x%p , length %zu", keys[i], val, vlen);
	}
	else
	    clicon_debug(dbglevel, "%s = NULL", keys[i]);
    }
    free(keys);

}

/*
 * clicon_option_readfile
 * read filename and add options to global registry
 */
static int 
clicon_option_readfile(clicon_hash_t *copt, const char *filename)
{
    struct stat st;
    char opt[1024], val[1024];
    char line[1024], *cp;
    FILE *f;
    int retval = -1;

    if (filename == NULL || !strlen(filename)){
	clicon_err(OE_UNIX, 0, "Not specified");
	return -1;
    }
    if (stat(filename, &st) < 0){
	clicon_err(OE_UNIX, errno, "%s", filename);
	return -1;
    }
    if (!S_ISREG(st.st_mode)){
	clicon_err(OE_UNIX, 0, "%s is not a regular file", filename);
	return -1;
    }
    if ((f = fopen(filename, "r")) == NULL) {
	clicon_err(OE_UNIX, errno, "configure file: %s", filename);
	return -1;
    }
    clicon_debug(2, "Reading config file %s", __FUNCTION__, filename);
    while (fgets(line, sizeof(line), f)) {
	if ((cp = strchr(line, '\n')) != NULL) /* strip last \n */
	    *cp = '\0';
	/* Trim out comments, strip whitespace, and remove CR */
	if ((cp = strchr(line, '#')) != NULL)
	    memcpy(cp, "\n", 2);
	if (sscanf(line, "%s %s", opt, val) < 2)
	    continue;
	if ((hash_add(copt, 
		      opt,
		      val,
		      strlen(val)+1)) == NULL)
	    goto catch;
    }
    retval = 0;
  catch:
    fclose(f);
    return retval;
}

/*
 * clicon_option_default
 * We require CLICON_APPDIR to be set.
 * Create a suite of other options from the top-level env variable
 * according to the layout of application dir:
 */
static int
clicon_option_default(clicon_hash_t  *copt)
{
    char           *appdir;
    struct stat     st;
    char           *val;
    int             retval = 0;
    
    if (!hash_lookup(copt, "CLICON_APPDIR")){
	clicon_err(OE_UNIX, 0, "CLICON_APPDIR not present in registry");
	return -1;
    }
    if ((appdir = hash_value(copt, "CLICON_APPDIR", NULL)) == NULL){
	clicon_err(OE_UNIX, 0, "CLICON_APPDIR not present in registry");
	return -1;
    }
    if (stat(appdir, &st) < 0){
	clicon_err(OE_UNIX, errno, "%s", appdir);
	return -1;
    }
    if (!S_ISDIR(st.st_mode)){
	clicon_err(OE_UNIX, ENOTDIR, "%s", appdir);
	return -1;
    }
    if (!hash_lookup(copt, "CLICON_CLI_DIR")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/frontend", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_CLI_DIR", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_BACKEND_DIR")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/backend", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_BACKEND_DIR", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_NETCONF_DIR")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/netconf", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_NETCONF_DIR", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_RUNNING_DB")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/db/running_db", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_RUNNING_DB", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_CANDIDATE_DB")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/db/candidate_db", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_CANDIDATE_DB", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_DBSPEC_TYPE")){
	if (hash_add(copt, "CLICON_DBSPEC_TYPE", "YANG", strlen("YANG")+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_DBSPEC_FILE")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/datamodel.spec", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_DBSPEC_FILE", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_YANG_DIR")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/yang", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_YANG_DIR", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_YANG_MODULE_MAIN")){
	if (hash_add(copt, "CLICON_YANG_MODULE_MAIN", "clicon", strlen("clicon")+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_ARCHIVE_DIR")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/archive", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_ARCHIVE_DIR", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_STARTUP_CONFIG")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/startup-config", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_STARTUP_CONFIG", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_SOCK")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/clicon.sock", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_SOCK", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_BACKEND_PIDFILE")){
	if ((val = chunk_sprintf(__FUNCTION__, "%s/clicon.pidfile", appdir)) == NULL)
	    goto catch;
	if (hash_add(copt, "CLICON_BACKEND_PIDFILE", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_SOCK_GROUP")){
	val = CLICON_SOCK_GROUP;
	if (hash_add(copt, "CLICON_SOCK_GROUP", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_CLI_MODE")){
	if (hash_add(copt, "CLICON_CLI_MODE", "base", strlen("base")+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_MASTER_PLUGIN")){
	val = CLICON_MASTER_PLUGIN;
	if (hash_add(copt, "CLICON_MASTER_PLUGIN", val, strlen(val)+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_CLI_GENMODEL")){
	if (hash_add(copt, "CLICON_CLI_GENMODEL", "1", strlen("1")+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_CLI_GENMODEL_TYPE")){
	if (hash_add(copt, "CLICON_CLI_GENMODEL_TYPE", "VARS", strlen("VARS")+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_AUTOCOMMIT")){
	if (hash_add(copt, "CLICON_AUTOCOMMIT", "0", strlen("0")+1) < 0)
	    goto catch;
    }
    /* Legacy is 1 but default should really be 0. New apps should use 0 */
    if (!hash_lookup(copt, "CLICON_CLI_VARONLY")){
	if (hash_add(copt, "CLICON_CLI_VARONLY", "1", strlen("1")+1) < 0)
	    goto catch;
    }
    if (!hash_lookup(copt, "CLICON_CLI_GENMODEL_COMPLETION")){
	if (hash_add(copt, "CLICON_CLI_GENMODEL_COMPLETION", "0", strlen("0")+1) < 0)
	    goto catch;
    }
    retval = 0;
  catch:
    unchunk_group(__FUNCTION__);
    return retval;
}


/*! Find appdir. Find and read configfile
 *
 1. First find APPDIR:
 1.1. command line arg (-a). else
 1.2. CLICON_APPDIR ENV variable. else (? i $home)
 1.3. In-compiled @APPDIR@ (eg /usr/local/share/clicon)
 2. The  set options:
 2.1. Set default options
 2.2. Read config-file (-f overrides default setting)
*/
int
clicon_options_main(clicon_handle h, int argc, char **argv)
{
    char          *configfile;
    char          *appdir = NULL;
    struct stat    st;
    clicon_hash_t *copt = clicon_options(h);

    /*
     * Setup appdir if not set by command-line above
     */
    if (!hash_lookup(copt, "CLICON_APPDIR")){ 
	char *env;
	if ((env = getenv("CLICON_APPDIR")))
	    hash_add(copt, "CLICON_APPDIR", env, strlen(env)+1);
	else {
#ifdef APPDIR /* See clicon_config.h */
	    if (APPDIR)
		hash_add(copt, "CLICON_APPDIR", APPDIR, strlen(APPDIR)+1);
#endif /* APPDIR */
	}
	if (hash_lookup(copt, "CLICON_APPDIR") == NULL){ 
	    clicon_err(OE_CFG, 0, "FATAL: APPDIR not set\n"
		       "Try -a; configure --with-appdir or CLICON_APPDIR env variable.");
	    return -1;
	}
    }
    appdir = hash_value(copt, "CLICON_APPDIR", NULL);
    if (stat(appdir, &st) < 0){
	clicon_err(OE_CFG, errno, "%s", appdir);
	return -1;
    }
    if (!S_ISDIR(st.st_mode)){
	clicon_err(OE_CFG, ENOTDIR, "%s", appdir);
	return -1;
    }
    clicon_debug(1, "CLICON_APPDIR=%s", appdir);
    /*
     * Set configure file if not set by command-line above
     */
    if (!hash_lookup(copt, "CLICON_CONFIGFILE")){ 
	char configfile[MAXPATHLEN];	
	snprintf(configfile, sizeof(configfile)-1, "%s/clicon.conf", appdir);
	hash_add(copt, "CLICON_CONFIGFILE", configfile, strlen(configfile)+1);
    }
    configfile = hash_value(copt, "CLICON_CONFIGFILE", NULL);
    clicon_debug(1, "CLICON_CONFIGFILE=%s", configfile);
    /* Set default options */
    if (clicon_option_default(copt) < 0)  /* init registry from file */
	return -1;

    /* Read configfile */
    if (clicon_option_readfile(copt, configfile) < 0)
	return -1;
    return 0;
}


/*! Check if a clicon option has a value
 */
int
clicon_option_exists(clicon_handle h, char *name)
{
    clicon_hash_t *copt = clicon_options(h);

    return (hash_lookup(copt, name) != NULL);
}

/*! Get a single string option string via handle
 *
 * @param   h       clicon_handle
 * @param   name    option name
 * @retval  NULL    If option not found, or value of option is NULL
 * @retval  string  value of option if found
 * clicon options should be strings.
 * To differentiate the two reasons why NULL may be returned, use function 
 * clicon_option_exists() before the call
 */
char *
clicon_option_str(clicon_handle h, char *name)
{
    clicon_hash_t *copt = clicon_options(h);

    if (hash_lookup(copt, name) == NULL)
//	clicon_err(OE_UNIX, 0, "Option not found %s", name);
	return NULL;
    return hash_value(copt, name, NULL);
}


/* 
 * clicon_option_str_set
 *
 * Set a single string option via handle 
 */
int
clicon_option_str_set(clicon_handle h, char *name, char *val)
{
    clicon_hash_t *copt = clicon_options(h);

    return hash_add(copt, name, val, strlen(val)+1)==NULL?-1:0;
}

/* 
 * clicon_option_int
 * get options as integer but stored as string
 * Note be saved as int but then option files are hard to handle.
 * Note also that -1 can be both error and value.
 * This means that it should be used together with clicon_option_exists().
 * Or explicitly check for values like 0 and 1.
 */
int
clicon_option_int(clicon_handle h, char *name)
{
    char *s;

    if ((s = clicon_option_str(h, name)) == NULL)
	return -1;
    return atoi(s);
}

/*
 * clicon_option_int_set
 * set option given as int.
 */
int
clicon_option_int_set(clicon_handle h, char *name, int val)
{
    char s[64];
    
    if (snprintf(s, sizeof(s)-1, "%u", val) < 0)
	return -1;
    return clicon_option_str_set(h, name, s);
}

/*-----------------------------------------------------------------
 * Specific option access functions.
 * These should be commonly accessible for all users of clicon lib 
 *-----------------------------------------------------------------*/

/* get unix domain socket */
char *
clicon_appdir(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_APPDIR");
}

char *
clicon_configfile(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_CONFIGFILE");
}

/*! Which syntax: YANG or KEY */
char *
clicon_dbspec_type(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_DBSPEC_TYPE");
}

/*! KEY database specification file*/
char *
clicon_dbspec_file(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_DBSPEC_FILE");
}

/*! YANG database specification directory */
char *
clicon_yang_dir(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_YANG_DIR");
}

/*! YANG main module */
char *
clicon_yang_module_main(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_YANG_MODULE_MAIN");
}

/* candidate database: get name */
char *
clicon_candidate_db(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_CANDIDATE_DB");
}

/* running database: get name */
char *
clicon_running_db(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_RUNNING_DB");
}

char *
clicon_backend_dir(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_BACKEND_DIR");
}

char *
clicon_cli_dir(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_CLI_DIR");
}

char *
clicon_netconf_dir(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_NETCONF_DIR");
}

char *
clicon_archive_dir(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_ARCHIVE_DIR");
}

char *
clicon_startup_config(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_STARTUP_CONFIG");
}

/* get unix domain socket */
char *
clicon_sock(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_SOCK");
}

char *
clicon_backend_pidfile(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_BACKEND_PIDFILE");
}

char *
clicon_sock_group(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_SOCK_GROUP");
}

char *
clicon_master_plugin(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_MASTER_PLUGIN");
}

char *
clicon_cli_group(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_CLI_GROUP");
}

/* return initial clicon cli mode */
char *
clicon_cli_mode(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_CLI_MODE");
}

/* Name of generated tree (use with '@' syntax in cligen spec) or 'OFF' */
int
clicon_cli_genmodel(clicon_handle h)
{
    return clicon_option_int(h, "CLICON_CLI_GENMODEL");
}

/* How to generate and show CLI syntax: VARS|ALL */
enum genmodel_type
clicon_cli_genmodel_type(clicon_handle h)
{
    char *s;
    enum genmodel_type gt = GT_ERR;

    if (!clicon_option_exists(h, "CLICON_CLI_GENMODEL_TYPE")){
	gt = GT_VARS;
	goto done;
    }
    s = clicon_option_str(h, "CLICON_CLI_GENMODEL_TYPE");
    if (strcmp(s, "NONE")==0)
	gt = GT_NONE;
    else
    if (strcmp(s, "VARS")==0)
	gt = GT_VARS;
    else
    if (strcmp(s, "ALL")==0)
	gt = GT_ALL;
  done:
    return gt;
}


/* eg -q option, dont print notifications on stdout */
char *
clicon_quiet_mode(clicon_handle h)
{
    return clicon_option_str(h, "CLICON_QUIET");
}

int
clicon_autocommit(clicon_handle h)
{
    return clicon_option_int(h, "CLICON_AUTOCOMMIT");
}

int
clicon_autocommit_set(clicon_handle h, int val)
{
    return clicon_option_int_set(h, "CLICON_AUTOCOMMIT", val);
}

int
clicon_cli_varonly(clicon_handle h)
{
    return clicon_option_int(h, "CLICON_CLI_VARONLY");
}

int
clicon_cli_varonly_set(clicon_handle h, int val)
{
    return clicon_option_int_set(h, "CLICON_CLI_VARONLY", val);
}

int
clicon_cli_genmodel_completion(clicon_handle h)
{
    return clicon_option_int(h, "CLICON_CLI_GENMODEL_COMPLETION");
}

/* 
 * Get dbspec (KEY variant)
 * Must use hash functions directly since they are not strings.
 */
dbspec_key *
clicon_dbspec_key(clicon_handle h)
{
    clicon_hash_t  *cdat = clicon_data(h);
    size_t          len;
    void           *p;

    if ((p = hash_value(cdat, "dbspec_key", &len)) != NULL)
	return *(dbspec_key **)p;
    return NULL;
}

/* 
 * Set dbspec (KEY variant)
 */
int
clicon_dbspec_key_set(clicon_handle h, dbspec_key *ds)
{
    clicon_hash_t  *cdat = clicon_data(h);

    /* It is the pointer to pt that should be copied by hash,
       so we send a ptr to the ptr to indicate what to copy.
     */
    if (hash_add(cdat, "dbspec_key", &ds, sizeof(ds)) == NULL)
	return -1;
    return 0;
}

/* 
 * Get dbspec (YANG variant)
 * Must use hash functions directly since they are not strings.
 */
yang_spec *
clicon_dbspec_yang(clicon_handle h)
{
    clicon_hash_t  *cdat = clicon_data(h);
    size_t          len;
    void           *p;

    if ((p = hash_value(cdat, "dbspec_yang", &len)) != NULL)
	return *(yang_spec **)p;
    return NULL;
}

/* 
 * Set dbspec (YANG variant)
 * ys must be a malloced pointer
 */
int
clicon_dbspec_yang_set(clicon_handle h, struct yang_spec *ys)
{
    clicon_hash_t  *cdat = clicon_data(h);

    /* It is the pointer to ys that should be copied by hash,
       so we send a ptr to the ptr to indicate what to copy.
     */
    if (hash_add(cdat, "dbspec_yang", &ys, sizeof(ys)) == NULL)
	return -1;
    return 0;
}


/* 
 * Get dbspec name as read from spec. Can be used in CLI '@' syntax.
 * XXX: this we muśt change,...
 */
char *
clicon_dbspec_name(clicon_handle h)
{
    if (!clicon_option_exists(h, "dbspec_name"))
	return NULL;
    return clicon_option_str(h, "dbspec_name");
}

/* 
 * Set dbspec name as read from spec. Can be used in CLI '@' syntax.
 */
int
clicon_dbspec_name_set(clicon_handle h, char *name)
{
    return clicon_option_str_set(h, "dbspec_name", name);
}
