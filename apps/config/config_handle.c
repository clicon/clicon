/*
 *  CVS Version: $Id: config_handle.c,v 1.10 2013/08/31 06:32:46 benny Exp $
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
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <regex.h>
#include <netinet/in.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include <clicon/clicon.h>

#include "clicon_backend_api.h"
#include "config_dbdiff.h"
#include "config_dbdep.h"
#include "config_client.h"

/* header part is copied from struct clicon_handle in lib/src/clicon_handle.c */

#define CLICON_MAGIC 0x99aafabe

#define handle(h) (assert(clicon_handle_check(h)==0),(struct backend_handle *)(h))

/*
 * config_handle
 * first part of this is header, same for clicon_handle and cli_handle.
 * Access functions for common fields are found in clicon lib: clicon_options.[ch]
 * This file should only contain access functions for the _specific_
 * entries in the struct below.
 */
struct backend_handle {
    int                      cb_magic;    /* magic (HDR)*/
    clicon_hash_t           *cb_copt;     /* clicon option list (HDR) */
    clicon_hash_t           *cb_data;     /* internal clicon data (HDR) */
    /* ------ end of common handle ------ */
    dbdep_t                 *cb_dbdep;    /* Database dependencies: commit checks */
};

/*
 * backend_handle_init
 * returns a clicon config handle for other CLICON API calls
 */
clicon_handle
backend_handle_init(void)
{
    return clicon_handle_init0(sizeof(struct backend_handle));
}

/*
 * clicon_backend_handle_exit
 * returns a clicon handle for other CLICON API calls
 */
int
backend_handle_exit(clicon_handle h)
{
    dbdeps_free(h); 
    clicon_handle_exit(h); /* frees h and options */
    return 0;
}

dbdep_t *
backend_dbdep(clicon_handle h)
{
    struct backend_handle *cb = handle(h);
    
    return cb->cb_dbdep;
}

int
backend_dbdep_set(clicon_handle h, dbdep_t *dbdep)
{
    struct backend_handle *cb = handle(h);

    cb->cb_dbdep = dbdep;
    return 0;
}

/*! Define a notify log message, part of the notify mechanism
 * 
 * Stream is a string used to qualify the event-stream. Distribute the event to
 * all clients registered to this backend.
 * We could extend this functionality to (1) the generic log system and (2) an event-log.
 * See also clicon_log(). We have chosen not to integrate this log-function with the
 * system-wide. Maybe we should?
 * XXX: placed here to be part of libclicon_backend, but really does not belong here?
 * See also: subscription_add()
 */
int
notify_log(char *stream, int level, char *format, ...)
{
    va_list              args;
    int                  len;
    char                *event = NULL;
    struct client_entry *ce;
    struct subscription *su;
    int                  retval = -1;

    va_start(args, format);
    len = vsnprintf(NULL, 0, format, args);
    va_end(args);
    /* Allocate event. */
    if ((event = malloc(len+1)) == NULL){
	clicon_err(OE_PLUGIN, errno, "malloc");
	goto done;
    }
    va_start(args, format);
    vsnprintf(event, len+1, format, args);
    va_end(args);

    /* Now go thru all clients(sessions), and all subscriptions and find matches */
    for (ce = ce_list; ce; ce = ce->ce_next)
	for (su = ce->ce_subscription; su; su = su->su_next)
	    if (strcmp(su->su_stream, stream) == 0)
		if (send_msg_notify(ce->ce_s, level, event) < 0)
		    goto done;
    retval = 0;
  done:
    if (event)
	free(event);
    return retval;
}

