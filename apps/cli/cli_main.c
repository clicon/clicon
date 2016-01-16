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
#define CLI_OPTS "hD:f:F:1s:u:d:m:cP:qptGLl:"

static int
cli_terminate(clicon_handle h)
{
    dbspec_key *dbspec;
    yang_spec      *yspec;

    if ((dbspec = clicon_dbspec_key(h)) != NULL)
	db_spec_free(dbspec);
    if ((yspec = clicon_dbspec_yang(h)) != NULL)
	yspec_free(yspec);
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
cli_signal_init (clicon_handle h)
{
	cli_signal_block(h);
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
    }
}

/*! Read database specification file.
 * Some complications: there are two variants of syntax: 
 * CLI and KEY set by CLICON_DBSPEC_TYPE
 * CLICON_DBSPEC_FILE  contains the syntax-spec. Either CLI or KEY.
 * In both cases, the dbspec is translated to both forms and saved as the dbspec and
 * dbspec2 options. 
 */
static int
dbspec_main_cli(clicon_handle h, int printspec, int printalt)
{
    char            *dbspec_type;
    int              retval = -1;
    
    if ((dbspec_type = clicon_dbspec_type(h)) == NULL){
	clicon_err(OE_FATAL, 0, "Dbspec type not set");
	goto quit;
    }
    if (strcmp(dbspec_type, "YANG") == 0){ /* Parse YANG spec syntax */
	if (yang_spec_main(h, stdout, printspec, printalt) < 0)
	    goto quit;
    }
    else
	if (strcmp(dbspec_type, "KEY") == 0){ /* Parse KEY syntax */
	    if (dbspec_key_main(h, stdout, printspec, printalt) < 0)
		goto quit;
	}
	else{
	    clicon_err(OE_FATAL, 0, "Unknown dbspec format: %s", dbspec_type);
	    goto quit;
	}
    retval = 0;
  quit:
    return retval;
}

static void
usage(char *argv0, clicon_handle h)
{
    char *confsock = clicon_sock(h);
    char *plgdir = clicon_cli_dir(h);

    fprintf(stderr, "usage:%s [options] [commands]\n"
	    "where commands is a CLI command or options passed to the main plugin\n" 
	    "where options are\n"
            "\t-h \t\tHelp\n"
    	    "\t-D <level> \tDebug\n"
	    "\t-f <file> \tConfig-file (mandatory)\n"
    	    "\t-F <file> \tRead commands from file (default stdin)\n"
	    "\t-1\t\tDont enter interactive mode\n"
	    "\t-s <file>\tDatabase spec file\n"
    	    "\t-u <sockpath>\tconfig UNIX domain path (default: %s)\n"
	    "\t-d <dir>\tSpecify plugin directory (default: %s)\n"
            "\t-m <mode>\tSpecify plugin syntax mode\n"
    	    "\t-c \t\tWrite to candidate directly, not via config engine\n"
    	    "\t-P <dbname> \tWrite to private database\n"
	    "\t-q \t\tQuiet mode, dont print greetings or prompt, terminate on ctrl-C\n"
	    "\t-p \t\tPrint database specification (YANG or KEY depending on CLICON_DBSPEC_TYPE)\n"
	    "\t-t \t\tPrint alternate spec translation (eg if YANG print KEY, if KEY print YANG)\n"
	    "\t-G \t\tPrint CLI syntax generated from dbspec (if CLICON_CLI_GENMODEL enabled)\n"
	    "\t-L \t\tDebug print dynamic CLI syntax including completions and expansions\n"
	    "\t-l <s|e|o> \tLog on (s)yslog, std(e)rr or std(o)ut (stderr is default)\n",
	    argv0,
	    confsock ? confsock : "none",
	    plgdir ? plgdir : "none"
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
    int          once;
    char	*tmp;
    char	*argv0 = argv[0];
    clicon_handle h;
    int          printspec = 0;
    int          printalt = 0;
    int          printgen  = 0;
    int          logclisyntax  = 0;
    int          help = 0;
    char        *treename;
    char        *running_db;
    int          logdst = CLICON_LOG_STDERR;

    /* Defaults */

    /* In the startup, logs to stderr & debug flag set later */
    clicon_log_init(__PROGRAM__, LOG_INFO, logdst); 
    /* Initiate CLICON handle */
    if ((h = cli_handle_init()) == NULL)
	goto done;
    if (cli_plugin_init(h) != 0) 
	goto done;
    dbtype = CANDIDATE_DB_SHARED;
    once = 0;
    private_db[0] = '\0';
    cli_set_send2backend(h, 1); /* send changes to config daemon */
    cli_set_comment(h, '#'); /* Default to handle #! clicon_cli scripts */

    /*
     * First-step command-line options for help, debug, config-file and log, 
     */
    optind = 1;
    opterr = 0;
    while ((c = getopt(argc, argv, CLI_OPTS)) != -1)
	switch (c) {
	case '?':
	case 'h':
	    /* Defer the call to usage() to later. Reason is that for helpful
	       text messages, default dirs, etc, are not set until later.
	       But this means that we need to check if 'help' is set before 
	       exiting, and then call usage() before exit.
	    */
	    help = 1; 
	    break;
	case 'D' : /* debug */
	    if (sscanf(optarg, "%d", &debug) != 1)
		usage(argv[0], h);
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

    /* Find and read configfile */
    if (clicon_options_main(h) < 0){
        if (help)
	  usage(argv[0], h);
	return -1;
    }

    /* Now rest of options */   
    opterr = 0;
    optind = 1;
    while ((c = getopt(argc, argv, CLI_OPTS)) != -1){
	switch (c) {
	case 'D' : /* debug */
	case 'f': /* config file */
	case 'l': /* Log destination */
	    break; /* see above */
	case 'F': /* read commands from file */
	    if (freopen(optarg, "r", stdin) == NULL){
		cli_output(stderr, "freopen: %s\n", strerror(errno));
		return -1;
	    }
	    break; 
	case '1' : /* Quit after reading database once - dont wait for events */
	    once = 1;
	    break;
	case 'u': /* config unix domain path/ ip host */
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
	case 'm': /* CLI syntax mode */
	    if (!strlen(optarg))
		usage(argv[0], h);
	    clicon_option_str_set(h, "CLICON_CLI_MODE", optarg);
	    break;
	case 'c' : /* No config daemon (used in bootstrapping and file load) */
	    cli_set_send2backend(h, 0);
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
	case 't' : /* Print alternative dbspec format (eg if YANG, print KEY) */
	    printalt++;
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
    cli_signal_init(h);

    /* Backward compatible mode, do not include keys in cgv-arrays in callbacks.
       Should be 0 but default is 1 since all legacy apps use 1
       Test legacy before shifting default to 0
     */
    cv_exclude_keys(clicon_cli_varonly(h)); 

    /* Parse db specification as cli*/
    if (dbspec_main_cli(h, printspec, printalt) < 0)
	goto done;

    /* Check plugin directory */
    if (clicon_cli_dir(h) == NULL){
	clicon_err(OE_PLUGIN, 0, "clicon_cli_dir not defined");
	goto done;
    }
    
    /* Create tree generated from dataspec */
    if (clicon_cli_genmodel(h)){
	yang_spec         *yspec;      /* yang spec */
	parse_tree         pt = {0,};  /* cli parse tree */
	char              *dbspec_type;

	if ((dbspec_type = clicon_dbspec_type(h)) == NULL){
	    clicon_err(OE_FATAL, 0, "Dbspec type not set");
	    goto done;
	}
	if (strcmp(dbspec_type, "YANG") == 0 ||
	    strcmp(dbspec_type, "KEY") == 0){ 
	    if ((yspec = clicon_dbspec_yang(h)) == NULL){
		clicon_err(OE_FATAL, 0, "No YANG DB_SPEC");
		goto done;
	    }
	    /* Create cli command tree from dbspec */
	    if (yang2cli(h, yspec, &pt, clicon_cli_genmodel_type(h)) < 0)
		goto done;
	}
	else{
	    clicon_err(OE_FATAL, 0, "Unknown dbspec format: %s", dbspec_type);
	    goto done;
	}
	if (strcmp(dbspec_type, "KEY")==0)  
	    treename = "datamodel:datamodel";  /* key syntax hardwire to 'datamodel' */
	else{ /* YANG */
	    if ((treename = clicon_dbspec_name(h)) == NULL){
		clicon_err(OE_FATAL, 0, "DB_SPEC has no name (insert name=\"myname\"; in .spec file)");
		goto done;
	    }
	    treename = chunk_sprintf(__FUNCTION__, "datamodel:%s", clicon_dbspec_name(h));
	} 
	cli_tree_add(h, treename, pt);
	if (printgen)
	    cligen_print(stdout, pt, 1);
    }

    /* Initialize cli syntax */
    if (cli_syntax_load(h) < 0)
	goto done;

    /* Set syntax mode if specified from command-line or config-file. */
    if (clicon_option_exists(h, "CLICON_CLI_MODE"))
	if ((tmp = clicon_cli_mode(h)) != NULL)
	    if (cli_set_syntax_mode(h, tmp) == 0) {
		fprintf(stderr, "FATAL: Failed to set syntax mode '%s'\n", tmp);
		goto done;
	    }

    if (!cli_syntax_mode(h)){
	fprintf (stderr, "FATAL: No cli mode set (use -m or CLICON_CLI_MODE)\n");
	goto done;
    }
    if (cli_tree(h, cli_syntax_mode(h)) == NULL){
	fprintf (stderr, "FATAL: No such cli mode: %s\n", cli_syntax_mode(h));
	goto done;
    }

    /* Initialize databases */    
    if ((running_db = clicon_running_db(h)) == NULL)
	goto done;

    if (strlen(private_db))
	clicon_option_str_set(h, "CLICON_CANDIDATE_DB", private_db);

    if (!cli_send2backend(h)) 
      if (db_init(running_db) < 0){
	fprintf (stderr, "FATAL: Could not init running_db. (Run as root?)\n");
	  goto done;
      }
    /* A client does not have access to the candidate (and running) 
       databases if both these conditions are true:
         1. clicon_sock_family(h) == AF_INET[6]
         2. cli_send2backend(h) == 1
    */
    if (clicon_sock_family(h) == AF_UNIX || cli_send2backend(h)==0)
	if (init_candidate_db(h, dbtype) < 0)
	    return -1;
    
    if (logclisyntax)
	cli_logsyntax_set(h, logclisyntax);

    if (debug)
	clicon_option_dump(h, debug);

    /* Call start function in all plugins before we go interactive 
       Pass all args after the standard options to plugin_start
     */

    tmp = *(argv-1);
    *(argv-1) = argv0;
    cli_plugin_start(h, argc+1, argv-1);
    *(argv-1) = tmp;

    /* Launch interfactive event loop, unless -1 */
    if (once == 0)
	cli_interactive(h);
  done:
    // Gets in your face if we log on stderr
    clicon_log_init(__PROGRAM__, LOG_INFO, 0); /* Log on syslog no stderr */
    clicon_log(LOG_NOTICE, "%s: %u Terminated\n", __PROGRAM__, getpid());
    cli_terminate(h);

    return 0;
}
