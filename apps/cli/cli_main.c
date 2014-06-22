/*
 *  CVS Version: $Id: cli_main.c,v 1.72 2013/09/19 15:00:52 olof Exp $
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
 */

#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#define __USE_GNU /* strverscmp */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <assert.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include <clicon/clicon.h>

#include "clicon_cli_api.h"

#include "cli_plugin.h"
#include "cli_generate.h"
#include "cli_common.h"
#include "cli_handle.h"

/* Command line options to be passed to getopt(3) */
#define CLI_OPTS "hD:f:F:a:s:u:d:og:m:bcP:qpGLl:t"

static int
terminate(clicon_handle h)
{
    struct db_spec *dbspec;
    parse_tree *pt;

    if ((dbspec = clicon_dbspec_key(h)) != NULL)
	db_spec_free(dbspec);
    if ((pt = clicon_dbspec_pt(h)) != NULL){
	pt_apply(*pt, dbspec_userdata_delete, h);
	cligen_parsetree_free(*pt, 1);
	free(pt);
    }
    cli_plugin_finish(h);    
    exit_candidate_db(h);
    cli_handle_exit(h);
    return 0;
}

/*
 * cli_sig_term
 * Unlink pidfile and quit
*/
static void
cli_sig_term(int arg)
{
    clicon_log(LOG_NOTICE, "%s: %u Terminated (killed by sig %d)", 
	    __PROGRAM__, getpid(), arg);
    exit(1);
}

/*
 * Setup signal handlers
 */
static void
cli_signal_init (void)
{
	cli_signal_block();
	set_signal(SIGTERM, cli_sig_term, NULL);
}

static void
cli_interactive(clicon_handle h)
{
    int     res;
    char   *cmd;
    char   *new_mode;
    int     result;
    
    /* Loop through all commands */
    while(!cli_exiting(h)) {
//	save_mode = 
	new_mode = cli_syntax_mode(h);
	if ((cmd = clicon_cliread(h)) == NULL) {
	    cli_set_exiting(h, 1); /* EOF */
	    break;
	}
	if ((res = clicon_parse(h, cmd, &new_mode, &result)) < 0)
	    break;
	if (res == CG_MATCH) {
	    /* If command loaded a new group, unload old */
	    cli_plugin_unload_oldgroup(h);
	}
    }
}

/*
 * set_default_syntax_group
 * In a given directory, if there is a single sub-directory in that list, return
 * it. Otherwise, return null.
 * Useful to get a default plugin-group.
 */
static int
set_default_syntax_group(clicon_handle h, char *dir)
{
    DIR	*dirp;
    struct dirent *dp;
    struct stat sb;
    char filename[MAXPATHLEN];
    char group[MAXPATHLEN];
    int uniquedir = 0;
    int retval = -1;

    if ((dirp = opendir(dir)) == 0){
	fprintf(stderr, "%s: opendir(%s) %s\n", __FUNCTION__, dir, strerror(errno));
	return 0;
    }
    while ((dp = readdir(dirp)) != NULL){
	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
	    continue;
	snprintf(filename, MAXPATHLEN, "%s/%s", dir, dp->d_name);
	if (lstat(filename, &sb) < 0){
	    fprintf(stderr, "%s: stat(%s) %s\n", 
		    __FUNCTION__, dp->d_name, strerror(errno));
	    goto quit;
	}
	if (S_ISDIR(sb.st_mode)){
	    if (uniquedir){
		uniquedir = 0;
		break;
	    }
	    else{
		strncpy(group, dp->d_name, sizeof(group)-1);
		uniquedir++;
	    }
	}
    }
    retval = 0;
  quit:
    closedir(dirp);
    if (uniquedir)
	clicon_option_str_set(h, "CLICON_CLI_GROUP", group);
    return retval;
}


/*
 * Read database specification file.
 * Some complications: there are two variants of syntax: 
 * CLI and KEY set by CLICON_DBSPEC_TYPE
 * CLICON_DBSPEC_FILE  contains the syntax-spec. Either CLI or KEY.
 * In both cases, the dbspec is translated to both forms and saved as the dbspec and
 * dbspec2 options. 
 */
static int
spec_main_cli(clicon_handle h, int printspec)
{
    char            *syntax;
    int              retval = -1;
    parse_tree      *pt;
    struct db_spec  *db_spec;
    struct db_spec  *db_spec2;
    char            *db_spec_file;
    parse_tree       pt2;
    struct stat      st;
    
    /* pt must be malloced since it is saved in options/hash-value */
    if ((pt = malloc(sizeof(*pt))) == NULL){
	clicon_err(OE_FATAL, errno, "malloc");
	goto quit;
    }
    memset(pt, 0, sizeof(*pt));

    /* Parse db specification */
    if ((db_spec_file = clicon_dbspec_file(h)) == NULL)
	goto quit;
    if (stat(db_spec_file, &st) < 0){
	clicon_err(OE_FATAL, errno, "CLICON_DBSPEC_FILE not found");
	goto quit;
    }
    clicon_debug(1, "CLICON_DBSPEC_FILE=%s", db_spec_file);
    syntax = clicon_dbspec_type(h);

    if ((syntax == NULL) || strcmp(syntax, "PT") == 0){ /* Parse CLI spec syntax */
	if (dbclispec_parse(h, db_spec_file, pt) < 0)
	    goto quit;
	/* The dbspec parse-tree is in pt*/
	if (printspec && debug) 
	    cligen_print(stdout, *pt, 0);
	clicon_dbspec_pt_set(h, pt);	
	/* Translate from the dbspec as cligen parse-tree to dbspec key fmt */
	if ((db_spec = dbspec_cli2key(pt)) == NULL) /* To dbspec */
	    goto quit;
	if (printspec)
	    db_spec_dump(stdout, db_spec);
	clicon_dbspec_key_set(h, db_spec);	
	if (debug>1){ 
	    if (dbspec_key2cli(h, db_spec, &pt2) < 0)
		goto quit;
	    if (printspec) /* XXX */
		cligen_print(stderr, pt2, 0);
	    cligen_parsetree_free(pt2, 1);
	}
    }
    else
	if (strcmp(syntax, "PT") == 0){ /* Parse KEY syntax */
	    if ((db_spec = db_spec_parse_file(db_spec_file)) == NULL)
		goto quit;
	    if (printspec && debug) 
		db_spec_dump(stdout, db_spec);
	    clicon_dbspec_key_set(h, db_spec);	
	    /* 1: single arg, 2: doublearg */
	    
	    if (dbspec_key2cli(h, db_spec, pt) < 0)
		goto quit;
	    if (printspec)
		cligen_print(stdout, *pt, 0);
	    clicon_dbspec_pt_set(h, pt);
	    if (debug>1){ 
		if ((db_spec2 = dbspec_cli2key(pt)) == NULL) /* To dbspec */
		    goto quit;
		if (printspec) /* XXX */
		    db_spec_dump(stderr, db_spec2);
	    }
	}
	else
	    if (strcmp(syntax, "YANG") == 0){ /* Parse KEY syntax */
		if (yang_parse(h, db_spec_file, pt) < 0)
		    goto quit;
		cligen_print(stdout, *pt, 0);
		clicon_log(LOG_INFO, "YANG PARSING OK");
		goto quit; /* XXX */
	    }

    retval = 0;
  quit:
    return retval;
}

static void
usage(char *argv0, clicon_handle h)
{
    char *appdir = clicon_appdir(h);
    char *conffile = clicon_configfile(h);
    char *confsock = clicon_sock(h);
    char *plgdir = clicon_cli_dir(h);
    char *plggrp = clicon_option_exists(h, "CLICON_CLI_GROUP")?clicon_cli_group(h):NULL;

    fprintf(stderr, "usage:%s [options] [commands]\n"
	    "where commands is a CLI command or options passed to the main plugin\n" 
	    "where options are\n"
            "\t-h \t\tHelp\n"
    	    "\t-D \t\tDebug\n"
    	    "\t-a <dir>\tApplication dir (default: %s)\n"
    	    "\t-f <file> \tConfig-file (default: %s)\n"
    	    "\t-F <file> \tRead commands from file (default stdin)\n"
    	    "\t-s <file>\tDatabase spec file\n"
    	    "\t-u <sockpath>\tconfig UNIX domain path (default: %s)\n"
	    "\t-d <dir>\tSpecify plugin directory (default: %s)\n"
	    "\t-g <group>\tSpecify input syntax group (default: %s)\n"
            "\t-m <mode>\tSpecify plugin syntax mode\n"
    	    "\t-c \t\tWrite to candidate directly, not via config engine\n"
    	    "\t-P <dbname> \tWrite to private database\n"
	    "\t-q \t\tQuiet mode, dont print greetings\n"
	    "\t-p \t\tPrint dbspec translation in cli/db format\n"
	    "\t-G \t\tPrint CLI syntax generated from dbspec (if enabled)\n"
	    "\t-l <s|e|o> \tLog on (s)yslog, std(e)rr or std(o)ut (stderr is default)\n"
	    "\t-L \t\tDebug print dynamic CLI syntax including completions and expansions\n"
	    "\t-t \t\tDump DTD of database spec and exit\n",
	    argv0,
	    appdir ? appdir : "none",
	    conffile ? conffile : "none",
	    confsock ? confsock : "none",
	    plgdir ? plgdir : "none",
	    plggrp ? plggrp : "none"
	);
    exit(1);
}


/*
 */
int
main(int argc, char **argv)
{
    char         c;    
    enum candidate_db_type dbtype;
    char         private_db[MAXPATHLEN];
    char        *plugin_dir;
    char	*tmp;
    char	*argv0 = argv[0];
    clicon_handle h;
    int          printspec = 0;
    int          printgen  = 0;
    int          logclisyntax  = 0;
    int          help = 0;
    char        *treename;
    int          logdst = CLICON_LOG_STDERR;
    int          dumpdtd = 0;

    /* Defaults */

    /* In the startup, logs to stderr & debug flag set later */
    clicon_log_init(__PROGRAM__, LOG_INFO, logdst); 
    /* Initiate CLICON handle */
    if ((h = cli_handle_init()) == NULL)
	goto quit;
    if (cli_plugin_init(h) != 0) 
	goto quit;
    dbtype = CANDIDATE_DB_SHARED;
    private_db[0] = '\0';
    cli_set_usedaemon(h, 1); /* send changes to config daemon */
    cli_set_comment(h, '#'); /* Default to handle #! clicon_cli scripts */

    /*
     * Command-line options for appdir, config-file, debug and help
     */
    optind = 1;
    opterr = 0;
    while ((c = getopt(argc, argv, CLI_OPTS)) != -1)
	switch (c) {
	case '?':
	case 'h':
	    /* Defer the call to usage() to later. Reason is that for helpful
	       text messages, default dirs, etc, are not set until later.
	       But this measn that we need to check if 'help' is set before 
	       exiting, and then call usage() before exit.
	    */
	    help = 1; 
	    break;
	case 'D' : /* debug */
	    if (sscanf(optarg, "%d", &debug) != 1)
		usage(argv[0], h);
	    break;
	case 'a': /* Register command line app-dir if any */
	    if (!strlen(optarg))
		usage(argv[0], h);
	    clicon_option_str_set(h, "CLICON_APPDIR", optarg);
	    break;
	case 'f': /* config file */
	    if (!strlen(optarg))
		usage(argv[0], h);
	    clicon_option_str_set(h, "CLICON_CONFIGFILE", optarg);
	    break;
	 case 'l': /* Log destination: s|e|o */
	   switch (optarg[0]){
	   case 's':
	     logdst = CLICON_LOG_SYSLOG;
	     break;
	   case 'e':
	     logdst = CLICON_LOG_STDERR;
	     break;
	   case 'o':
	     logdst = CLICON_LOG_STDOUT;
	     break;
	   default:
	     usage(argv[0], h);
	   }
	     break;
	}
    /* 
     * Logs, error and debug to stderr or syslog, set debug level
     */
    clicon_log_init(__PROGRAM__, debug?LOG_DEBUG:LOG_INFO, logdst);

    clicon_debug_init(debug, NULL); 

    /* Find appdir. Find and read configfile */
    if (clicon_options_main(h, argc, argv) < 0)
	return -1;

    /* Now rest of options */   
    opterr = 0;
    optind = 1;
    while ((c = getopt(argc, argv, CLI_OPTS)) != -1){
	switch (c) {
	case 'D' : /* debug */
	case 'a' : /* appdir */
	case 'f': /* config file */
	case 'l': /* Log destination */
	    break; /* see above */
	case 'F': /* read commands from file */
	    if (freopen(optarg, "r", stdin) == NULL){
		cli_output(stderr, "freopen: %s\n", strerror(errno));
		return -1;
	    }
	    break; 
	case 'u': /* config unix domain path */
	    if (!strlen(optarg))
		usage(argv[0], h);
	    clicon_option_str_set(h, "CLICON_SOCK", optarg);
	    break;
	case 'd':  /* Plugin directory: overrides configfile */
	    if (!strlen(optarg))
		usage(argv[0], h);
	    clicon_option_str_set(h, "CLICON_CLI_DIR", optarg);
	    break;
	case 's':  /* db spec file */
	    if (!strlen(optarg))
		usage(argv[0], h);
	    clicon_option_str_set(h, "CLICON_DBSPEC_FILE", optarg);
	    break;
	case 'g' : /* CLI syntax group */
	    if (!strlen(optarg))
		usage(argv[0], h);
	    clicon_option_str_set(h, "CLICON_CLI_GROUP", optarg);
	    break;
	case 'm': /* CLI syntax mode */
	    if (!strlen(optarg))
		usage(argv[0], h);
	    clicon_option_str_set(h, "CLICON_CLI_MODE", optarg);
	    break;
	case 'c' : /* No config daemon (used in bootstrapping and file load) */
	    cli_set_usedaemon(h, 0);
	    break;
	case 'P' : /* load to private database with given name */
	    dbtype = CANDIDATE_DB_PRIVATE;
	    clicon_option_str_set(h, "CLICON_CANDIDATE_DB", optarg); /* override default */
	    break;
	case 'q' : /* Quiet mode */
	    clicon_option_str_set(h, "CLICON_QUIET", "on"); 
	    break;
	case 'p' : /* Print spec */
	    printspec++;
	    break;
	case 'G' : /* Print generated CLI syntax */
	    printgen++;
	    break;
	case 'L' : /* Debug print dynamic CLI syntax */
	    logclisyntax++;
	    break;
	case 't' : /* Dump dtd of dbspec */
	    dumpdtd++;
	    break;
	default:
	    usage(argv[0], h);
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    /* Defer: Wait to the last minute to print help message */
    if (help)
	usage(argv[0], h);

    /* Setup signal handlers */
    cli_signal_init();

    /* Backward compatible mode, do not include keys in cgv-arrays in callbacks.
       Should be 0 but default is 1 since all legacy apps use 1
       Test legacy before shifting default to 0
     */
    cv_exclude_keys(clicon_cli_varonly(h)); 

    /* Parse db specification as cli*/
    if (spec_main_cli(h, printspec) < 0)
	goto quit;

    if (dumpdtd) { /* Dump database specification as DTD */
	parse_tree *pt;
	if ((pt = clicon_dbspec_pt(h)) != NULL){
	    if (dbspec2dtd(stdout, pt) < 0)
		goto quit;
	}
	exit(1);
    }

    /* Check plugin directory */
    if ((plugin_dir = clicon_cli_dir(h)) == NULL)
	goto quit;
    
    /* Check syntax group */
    if (!clicon_option_exists(h, "CLICON_CLI_GROUP"))
	if (set_default_syntax_group(h, plugin_dir) < 0)
	    goto quit;
    if (!clicon_option_exists(h, "CLICON_CLI_GROUP"))
	if (clicon_option_str_set(h, "CLICON_CLI_GROUP", "") < 0) /* Set empty */
	    goto quit;
    /* Create tree generated from dataspec */
    if (clicon_cli_genmodel(h)){
	parse_tree        *pts;        /* dbspec cli */
	parse_tree         pt = {0,};   /* cli parse tree */

	if (strcmp(clicon_dbspec_type(h), "PT")==0){
		if ((treename = clicon_dbspec_name(h)) == NULL){
		    clicon_err(OE_FATAL, 0, "DB_SPEC has no name (insert name=\"myname\"; in .spec file)");
		    goto quit;
		}
	}
	else /* key syntax hardwire to 'datamodel' */
	    treename = "datamodel";
	if ((pts = clicon_dbspec_pt(h)) == NULL){
	    clicon_err(OE_FATAL, 0, "No DB_SPEC");
	    goto quit;
	}
	if (pts->pt_len == 0){
	    clicon_err(OE_FATAL, 0, "DB_SPEC length is zero");
	    goto quit;
	}
	/* Create cli command tree from dbspec */
	if (dbspec2cli(h, pts, &pt, clicon_cli_genmodel_type(h)) < 0)
	    goto quit;
	cli_tree_add(h, treename, pt);
	if (printgen)
	    cligen_print(stdout, pt, 1);
    }

    /* Syntax group is 'heavyweight' and cli mode is 'lightweight' */
    /* Initialize cli syntax group */
    if (cli_syntax_group_load(h, clicon_cli_group(h)) < 0)
	goto quit;

    /* Set syntax mode if specified from command-line or config-file. */
    if (clicon_option_exists(h, "CLICON_CLI_MODE"))
	if ((tmp = clicon_cli_mode(h)) != NULL)
	    if (cli_set_syntax_mode(h, tmp) == 0) {
		fprintf(stderr, "FATAL: Failed to set syntax mode '%s'\n", tmp);
		goto quit;
	    }

    if (!cli_syntax_mode(h)){
	fprintf (stderr, "FATAL: No cli mode set (use -m or CLICON_CLI_MODE)\n");
	goto quit;
    }
    if (cli_tree(h, cli_syntax_mode(h)) == NULL){
	fprintf (stderr, "FATAL: No such cli mode: %s\n", cli_syntax_mode(h));
	goto quit;
    }

    /* Initialize databases */    
    if (clicon_running_db(h) == NULL)
	goto quit;

    if (strlen(private_db))
	clicon_option_str_set(h, "CLICON_CANDIDATE_DB", private_db);

    if (init_candidate_db(h, dbtype) < 0)
	return -1;
    
    if (logclisyntax)
	cli_logsyntax_set(h, logclisyntax);

    if (debug>1)
	clicon_option_dump(h, stderr);

    /* Call start function is all plugins before we go interactive 
       Pass all args after the standard options to plugin_start
     */

    tmp = *(argv-1);
    *(argv-1) = argv0;
    cli_plugin_start(h, argc+1, argv-1);
    *(argv-1) = tmp;

    /* Lauch interfactive event loop */
    cli_interactive(h);
  quit:
    // Gets in your face if we log on stderr
    clicon_log_init(__PROGRAM__, LOG_INFO, 0); /* Log on syslog no stderr */
    clicon_log(LOG_NOTICE, "%s: %u Terminated\n", __PROGRAM__, getpid());
    terminate(h);

    return 0;
}
