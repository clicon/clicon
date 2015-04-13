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
 * Event handling and loop
 */

#ifdef HAVE_CONFIG_H
#include "clicon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>

#include "clicon_queue.h"
#include "clicon_log.h"
#include "clicon_err.h"
#include "clicon_event.h"

/*
 * Constants
 */
#define EVENT_STRLEN 32

/*
 * Types
 */
struct event_data{
    struct event_data *e_next;     /* next in list */
    int (*e_fn)(int, void*);            /* function */
    enum {EVENT_FD, EVENT_TIME} e_type;        /* type of event */
    int e_fd;                      /* File descriptor */
    struct timeval e_time;         /* Timeout */
    void *e_arg;                   /* function argument */
    char e_string[EVENT_STRLEN];             /* string for debugging */
};

/*
 * Internal variables
 */
static struct event_data *ee = NULL;
static struct event_data *ee_timers = NULL;
static int _clicon_exit = 0;

/*! For signal handlers: instead of doing exit, set a global variable to exit
 * Status is then checked in event_loop.
 * Note it maybe would be better to do use on a handle basis, bit a signal
 * handler is global
 */
int
clicon_exit_set(void)
{
    _clicon_exit++;
    return 0;
}

/*! Get the status of global exit variable, usually set by signal handlers
 */
int
clicon_exit_get(void)
{
    return _clicon_exit;
}

/*! Register a callback function to be called on input on a file descriptor.
 *
 * @param[in]  fd  File descriptor
 * @param[in]  fn  Function to call when input available on fd
 * @param[in]  arg Argument to function fn
 * @param[in]  str Describing string for logging
 * @code
 * int fn(int fd, void *arg){
 * }
 * event_reg_fd(fd, fn, (void*)42, "call fn on input on fd");
 * @endcode 
 */
int
event_reg_fd(int fd, int (*fn)(int, void*), void *arg, char *str)
{
    struct event_data *e;

    if ((e = (struct event_data *)malloc(sizeof(struct event_data))) == NULL){
	clicon_err(OE_EVENTS, errno, "malloc");
	return -1;
    }
    memset(e, 0, sizeof(struct event_data));
    strncpy(e->e_string, str, EVENT_STRLEN);
    e->e_fd = fd;
    e->e_fn = fn;
    e->e_arg = arg;
    e->e_type = EVENT_FD;
    e->e_next = ee;
    ee = e;
    clicon_debug(2, "%s, registering %s", __FUNCTION__, e->e_string);
    return 0;
}

/*! Deregister a file descriptor callback
 * @param[in]  s   File descriptor
 * @param[in]  fn  Function to call when input available on fd
 * Note: deregister when exactly function and socket match, not argument
 * @see event_reg_fd
 * @see event_unreg_timeout
 */
int
event_unreg_fd(int s, int (*fn)(int, void*))
{
    struct event_data *e, **e_prev;
    int found = 0;

    e_prev = &ee;
    for (e = ee; e; e = e->e_next){
	if (fn == e->e_fn && s == e->e_fd) {
	    found++;
	    *e_prev = e->e_next;
	    free(e);
	    break;
	}
	e_prev = &e->e_next;
    }
    return found?0:-1;
}

/*! Call a callback function at an absolute time
 * @param[in]  t   Absolute (not relative!) timestamp when callback is called
 * @param[in]  fn  Function to call at time t
 * @param[in]  arg Argument to function fn
 * @param[in]  str Describing string for logging
 * @code
 * int fn(int d, void *arg){
 *   struct timeval t, t1;
 *   gettimeofday(&t, NULL);
 *   t1.tv_sec = 1; t1.tv_usec = 0;
 *   timeradd(&t, &t1, &t);
 *   event_reg_timeout(t, fn, NULL, "call every second");
 * } 
 * @endcode 
 * 
 * Note that the timestamp is an absolute timestamp, not relative.
 * Note also that the callback is not periodic, you need to make a new 
 * registration for each period, see example above.
 * Note also that the first argument to fn is a dummy, just to get the same
 * signatute as for file-descriptor callbacks.
 * @see event_reg_fd
 * @see event_unreg_timeout
 */
int
event_reg_timeout(struct timeval t,  int (*fn)(int, void*), 
		  void *arg, char *str)
{
    struct event_data *e, *e1, **e_prev;

    if ((e = (struct event_data *)malloc(sizeof(struct event_data))) == NULL){
	clicon_err(OE_EVENTS, errno, "malloc");
	return -1;
    }
    memset(e, 0, sizeof(struct event_data));
    strncpy(e->e_string, str, EVENT_STRLEN);
    e->e_fn = fn;
    e->e_arg = arg;
    e->e_type = EVENT_TIME;
    e->e_time = t;
    /* Sort into right place */
    e_prev = &ee_timers;
    for (e1=ee_timers; e1; e1=e1->e_next){
	if (timercmp(&e->e_time, &e1->e_time, <))
	    break;
	e_prev = &e1->e_next;
    }
    e->e_next = e1;
    *e_prev = e;
    clicon_debug(2, "event_reg_timeout: %s", str); 
    return 0;
}

/*! Deregister a timeout callback as previosly registered by event_reg_timeout()
 * Note: deregister when exactly function and function arguments match, not time. So you
 * cannot have same function and argument callback on different timeouts. This is a little
 * different from event_unreg_fd.
 * @param[in]  fn  Function to call at time t
 * @param[in]  arg Argument to function fn
 * @see event_reg_timeout
 * @see event_unreg_fd
 */
int
event_unreg_timeout(int (*fn)(int, void*), void *arg)
{
    struct event_data *e, **e_prev;
    int found = 0;

    e_prev = &ee_timers;
    for (e = ee_timers; e; e = e->e_next){
	if (fn == e->e_fn && arg == e->e_arg) {
	    found++;
	    *e_prev = e->e_next;
	    free(e);
	    break;
	}
	e_prev = &e->e_next;
    }
    return found?0:-1;
}

/*! Dispatch file descriptor events (and timeouts) by invoking callbacks.
 * There is an issue with fairness that timeouts may take over all events
 * One could try to poll the file descriptors after a timeout?
 */
int
event_loop(void)
{
    struct event_data *e, *e_next;
    int n;
    struct timeval t, t0, tnull={0,};
    fd_set fdset;
    int retval = -1;

    while (!clicon_exit_get()){
	FD_ZERO(&fdset);
	for (e=ee; e; e=e->e_next)
	    if (e->e_type == EVENT_FD)
		FD_SET(e->e_fd, &fdset);
	if (ee_timers != NULL){
	    gettimeofday(&t0, NULL);
	    timersub(&ee_timers->e_time, &t0, &t); 
	    if (t.tv_sec < 0)
		n = select(FD_SETSIZE, &fdset, NULL, NULL, &tnull); 
	    else
		n = select(FD_SETSIZE, &fdset, NULL, NULL, &t); 
	}
	else
	    n = select(FD_SETSIZE, &fdset, NULL, NULL, NULL); 
	if (clicon_exit_get())
	    break;
	if (n == -1) {
	    if (errno == EINTR){
		clicon_debug(1, "%s select: %s", __FUNCTION__, strerror(errno));
		clicon_err(OE_EVENTS, errno, "%s select1: %s", __FUNCTION__, strerror(errno));
		retval = 0;
		goto err;
	    }
	    else
		clicon_err(OE_EVENTS, errno, "%s select2", __FUNCTION__);
	    goto err;
	}
	if (n==0){ /* Timeout */
	    e = ee_timers;
	    ee_timers = ee_timers->e_next;
	    clicon_debug(2, "%s timeout: %s[%x]", 
		    __FUNCTION__, e->e_string, e->e_arg);
	    if ((*e->e_fn)(0, e->e_arg) < 0){
		free(e);
		goto err;
	    }
	    free(e);
	}
	for (e=ee; e; e=e_next){
	    if (clicon_exit_get())
		break;
	    e_next = e->e_next;
	    if(e->e_type == EVENT_FD && FD_ISSET(e->e_fd, &fdset)){
		clicon_debug(2, "%s: FD_ISSET: %s[%x]", 
			__FUNCTION__, e->e_string, e->e_arg);
		if ((*e->e_fn)(e->e_fd, e->e_arg) < 0)
		    goto err;
	    }
	}
	continue;
      err:
	break;
    }
    clicon_debug(1, "%s done:", __FUNCTION__);
    return retval;
}

