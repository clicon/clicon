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

/*! Read pidfile and return pid, if any
 *
 * @param[in]  pidfile  Name of pidfile
 * @param[out] pid0     Process id of (eventual) existing daemon process
 * @retval    0         OK. if pid > 0 old process exists w that pid
 * @retval    -1        Error, and clicon_err() called
 */
int
pidfile_get(char *pidfile, pid_t *pid0)
{
    FILE   *f;
    char   *ptr;
    char    buf[32];
    pid_t   pid;

    if ((f = fopen (pidfile, "r")) != NULL){
	ptr = fgets(buf, sizeof(buf), f);
	fclose (f);
	if (ptr != NULL && (pid = atoi (ptr)) > 1) {
	    if (kill (pid, 0) == 0 || errno != ESRCH) {
		/* Yes there is a process */
		*pid0 = pid;
		return 0;
	    }
	}
    }
    *pid0 = 0;
    return 0;
}

/*! Given a pid, kill that process
 *
 * @param[in] pid   Process id
 * @retval    0     Killed OK
 * @retval    -1    Could not kill. 
 * Maybe shouldk not belong to pidfile code,..
 */
int
pidfile_zapold(pid_t pid)
{
    clicon_log(LOG_NOTICE, "Killing old daemon with pid: %d", pid);
    killpg(pid, SIGTERM);
    kill(pid, SIGTERM);
    sleep(1); /* check again */
    if ((kill (pid, 0)) != 0 && errno == ESRCH) /* Nothing there */
	;
    else{ /* problem: couldnt kill it */
	clicon_err(OE_DEMON, errno, "Killing old demon");
	return -1;
    }
    return 0;
}

/*! Write a pid-file
 *
 * @param[in] pidfile   Name of pidfile
 * @param[in] kill_it   If set, kill existing process otherwise return pid
 */
int
pidfile_write(char *pidfile)
{
    FILE *f = NULL;
    int   retval = -1;

    /* Here, there should be no old agent and no pidfile */
    if ((f = fopen(pidfile, "wb")) == NULL){
	if (errno == EACCES)
	    clicon_err(OE_DEMON, errno, "Creating pid-file %s (Try run as root?)", pidfile);
	else
	    clicon_err(OE_DEMON, errno, "Creating pid-file %s", pidfile);
	goto done;
    } 
    if ((retval = fprintf(f, "%ld\n", (long) getpid())) < 1){
	clicon_err(OE_DEMON, errno, "Could not write pid to %s", pidfile);
	goto done;
    }
    clicon_debug(1, "Opened pidfile %s with pid %d", pidfile, getpid());
    retval = 0;
 done:
    if (f != NULL)
	fclose(f);
    return retval;
}
