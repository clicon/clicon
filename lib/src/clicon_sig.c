/*
 *  CVS Version: $Id: clicon_sig.c,v 1.3 2013/08/01 09:15:47 olof Exp $
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

 */

#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>

/* clicon */
#include "clicon_err.h"
#include "clicon_log.h"
#include "clicon_sig.h"

/*
 * Set a signal handler.
 */
int
set_signal(int signo, void (*handler)(int), void (**oldhandler)(int))
{
#if defined(HAVE_SIGACTION)
    struct sigaction sold, snew;

    snew.sa_handler = handler;
    sigemptyset(&snew.sa_mask);
     snew.sa_flags = 0;
     if (sigaction (signo, &snew, &sold) < 0){
	clicon_err(OE_UNIX, errno, "sigaction");
	return -1;
     }
     if (oldhandler)
	 *oldhandler = sold.sa_handler;
    return 0;
#elif defined(HAVE_SIGVEC)
    assert(0);
    return 0;
#endif
}


/* 
 * Block signal. If 'sig' is 0, block all signals
 */
void
clicon_signal_block (int sig)
{
	sigset_t
		set;

	sigemptyset (&set);
	if (sig)
		sigaddset (&set, sig);
	else 
		sigfillset (&set);

	sigprocmask (SIG_BLOCK, &set, NULL);
}

/* 
 * Unblock signal. If 'sig' is 0, unblock all signals
 */
void
clicon_signal_unblock (int sig)
{
	sigset_t
		set;

	sigemptyset (&set);
	if (sig)
		sigaddset (&set, sig);
	else 
		sigfillset (&set);

	sigprocmask (SIG_UNBLOCK, &set, NULL);
}

/*
 * pidfile_check
 * Check if there is a pid file.
 * If so, kill the old process is specified.
 * Then write new pidfile.
 * Maybe we should do the opposite: if there exists one dont start this one?
 */
pid_t
pidfile_check(char *pidfile, int kill_it)
{
    FILE *f;
    char   *ptr;
    char buf[32];
    pid_t pid;

    if ((f = fopen (pidfile, "r")) != NULL){
	ptr = fgets(buf, sizeof(buf), f);
	fclose (f);
	if (ptr != NULL && (pid = atoi (ptr)) > 1) {
	    if (kill (pid, 0) == 0 || errno != ESRCH) {
		if (kill_it == 0) /* Don't kill, report pid */
		    return pid;
		clicon_debug(1, "Killing old daemon with pid: %d", pid);
		killpg(pid, SIGTERM);
		kill(pid, SIGTERM);
		sleep(1); /* check again */
		if ((kill (pid, 0)) != 0 && errno == ESRCH) /* Nothing there */
		    ;
		else{ /* problem: couldnt kill it */
		    clicon_err(OE_DEMON, errno, "Killing old demon");
		    return -1;
		}
	    }
	    unlink(pidfile);	
	}
    }
    /* Here, there should be no old agent and no pidfile */
    if ((f = fopen(pidfile, "wb")) == NULL){
	if (errno == EACCES)
	    clicon_err(OE_DEMON, errno, "Creating pid-file %s (Try run as root?)", pidfile);
	else
	    clicon_err(OE_DEMON, errno, "Creating pid-file %s", pidfile);
	return -1;
    } 
    fprintf(f, "%ld\n", (long) getpid());
    fclose(f);
    clicon_debug(1, "Opened pidfile %s with pid %d", pidfile, getpid());
    
    return 0;
}
