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
#include <unistd.h>
#include <assert.h>
#include <fnmatch.h>
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
#include "config_handle.h"

/* header part is copied from struct clicon_handle in lib/src/clicon_handle.c */

#define CLICON_MAGIC 0x99aafabe

#define handle(h) (assert(clicon_handle_check(h)==0),(struct backend_handle *)(h))

/*
 * backend_handle
 * first part of this is header, same for clicon_handle and cli_handle.
 * Access functions for common fields are found in clicon lib: clicon_options.[ch]
 * This file should only contain access functions for the _specific_
 * entries in the struct below.
 */
struct backend_handle {
    int                      cb_magic;     /* magic (HDR)*/
    clicon_hash_t           *cb_copt;      /* clicon option list (HDR) */
    clicon_hash_t           *cb_data;      /* internal clicon data (HDR) */
    /* ------ end of common handle ------ */
    dbdep_t                 *cb_dbdep;     /* Database dependencies: commit checks */
    struct client_entry     *cb_ce_list;   /* The client list */
    int                      cb_ce_nr;     /* Number of clients, just increment */
    struct handle_subscription *cb_subscription; /* Event subscription list */
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
    struct client_entry   *ce;

    dbdeps_free(h); 
    /* only delete client structs, not close sockets, etc, see backend_client_rm */
    while ((ce = backend_client_list(h)) != NULL)
	backend_client_delete(h, ce);
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

/*! Notify event and distribute to all registered clients
 * 
 * @param[in]  h       Clicon handle
 * @param[in]  stream  Name of event stream. CLICON is predefined as LOG stream
 * @param[in]  level   Event level (not used yet)
 * @param[in]  event   Actual message as text format
 *
 * Stream is a string used to qualify the event-stream. Distribute the
 * event to all clients registered to this backend.  
 * XXX: event-log NYI.  
 * @see also subscription_add()
 * @see also backend_notify_xml()
 */
int
backend_notify(clicon_handle h, char *stream, int level, char *event)
{
    struct client_entry        *ce;
    struct client_subscription *su;
    struct handle_subscription *hs;
    int                  retval = -1;

    /* First thru all clients(sessions), and all subscriptions and find matches */
    for (ce = backend_client_list(h); ce; ce = ce->ce_next)
	for (su = ce->ce_subscription; su; su = su->su_next)
	    if (strcmp(su->su_stream, stream) == 0){
		if (fnmatch(su->su_filter, event, 0) == 0)
		    if (send_msg_notify(ce->ce_s, level, event) < 0)
			goto done;
	    }
    /* Then go thru all global (handle) subscriptions and find matches */
    hs = NULL;
    while ((hs = subscription_each(h, hs)) != NULL){
	if (hs->hs_format != MSG_NOTIFY_TXT)
	    continue;
	if (strcmp(hs->hs_stream, stream))
	    continue;
	if (fnmatch(hs->hs_filter, event, 0) == 0)
	    if ((*hs->hs_fn)(h, event, hs->hs_arg) < 0)
		goto done;
    }
    retval = 0;
  done:
    return retval;
}

/*! Notify event and distribute to all registered clients
 * 
 * @param[in]  h       Clicon handle
 * @param[in]  stream  Name of event stream. CLICON is predefined as LOG stream
 * @param[in]  level   Event level (not used yet)
 * @param[in]  event   Actual message as xml tree
 *
 * Stream is a string used to qualify the event-stream. Distribute the
 * event to all clients registered to this backend.  
 * XXX: event-log NYI.  
 * @see also subscription_add()
 * @see also backend_notify()
 */
int
backend_notify_xml(clicon_handle h, char *stream, int level, cxobj *x)
{
    struct client_entry *ce;
    struct client_subscription *su;
    int                  retval = -1;
    cbuf                *cb = NULL;
    struct handle_subscription *hs;

    /* Now go thru all clients(sessions), and all subscriptions and find matches */
    for (ce = backend_client_list(h); ce; ce = ce->ce_next)
	for (su = ce->ce_subscription; su; su = su->su_next)
	    if (strcmp(su->su_stream, stream) == 0){
		if (strlen(su->su_filter)==0 || xpath_first(x, su->su_filter) != NULL){
		    if (cb==NULL){
			if ((cb = cbuf_new()) == NULL){
			    clicon_err(OE_PLUGIN, errno, "cbuf_new");
			    goto done;
			}
			if (clicon_xml2cbuf(cb, x, 0, 0) < 0)
			    goto done;
		    }
		    if (send_msg_notify(ce->ce_s, level, cbuf_get(cb)) < 0)
			goto done;
		}
	    }
    /* Then go thru all global (handle) subscriptions and find matches */
    /* XXX: x contains name==dk-ore, but filter is 
       id==/[userid=d2d5e46c-c6f9-42f3-9a69-fb52fe60940d] */
    hs = NULL;
    while ((hs = subscription_each(h, hs)) != NULL){
	if (hs->hs_format != MSG_NOTIFY_XML)
	    continue;
	if (strcmp(hs->hs_stream, stream))
	    continue;
	if (strlen(hs->hs_filter)==0 || xpath_first(x, hs->hs_filter) != NULL)
	    if ((*hs->hs_fn)(h, x, hs->hs_arg) < 0)
		goto done;
    }
    retval = 0;
  done:
    if (cb)
	cbuf_free(cb);
    return retval;

}

struct client_entry *
backend_client_add(clicon_handle h, struct sockaddr *addr)
{
    struct backend_handle *cb = handle(h);
    struct client_entry   *ce;

    if ((ce = (struct client_entry *)malloc(sizeof(*ce))) == NULL){
	clicon_err(OE_PLUGIN, errno, "malloc");
	return NULL;
    }
    memset(ce, 0, sizeof(*ce));
    ce->ce_nr = cb->cb_ce_nr++;
    memcpy(&ce->ce_addr, addr, sizeof(*addr));
    ce->ce_next = cb->cb_ce_list;
    cb->cb_ce_list = ce;
    return ce;
}

struct client_entry *
backend_client_list(clicon_handle h)
{
    struct backend_handle *cb = handle(h);

    return cb->cb_ce_list;
}

/*! Actually remove client from list
 * See also backend_client_rm()
 */
int
backend_client_delete(clicon_handle h, struct client_entry *ce)
{
    struct client_entry   *c;
    struct client_entry  **ce_prev;
    struct backend_handle *cb = handle(h);

    ce_prev = &cb->cb_ce_list;
    for (c = *ce_prev; c; c = c->ce_next){
	if (c == ce){
	    *ce_prev = c->ce_next;
	    free(ce);
	    break;
	}
	ce_prev = &c->ce_next;
    }
    return 0;
}

/*! Add subscription given stream name, callback and argument 
 * @param[in]  h      Clicon handle
 * @param[in]  stream Name of event stream
 * @param[in]  format Expected format of event, eg text or xml
 * @param[in]  filter Filter to match event, depends on format, eg xpath for xml
 * @param[in]  fn     Callback when event occurs
 * @param[in]  arg    Argument to use with callback. Also handle when deleting
 * Note that arg is not a real handle.
 * @see subscription_delete
 * @see subscription_each
 */
struct handle_subscription *
subscription_add(clicon_handle        h,
		 char                *stream, 
		 enum format_enum     format,
		 char                *filter,
		 subscription_fn_t    fn,
		 void                *arg)
{
    struct backend_handle *cb = handle(h);
    struct handle_subscription *hs = NULL;

    if ((hs = malloc(sizeof(*hs))) == NULL){
	clicon_err(OE_PLUGIN, errno, "malloc");
	goto done;
    }
    memset(hs, 0, sizeof(*hs));
    hs->hs_stream = strdup(stream);
    hs->hs_format = format;
    hs->hs_filter = strdup(filter);
    hs->hs_next   = cb->cb_subscription;
    hs->hs_fn     = fn;
    hs->hs_arg    = arg;
    cb->cb_subscription = hs;
  done:
    return hs;
}

/*! Delete subscription given stream name, callback and argument 
 * @param[in]  h      Clicon handle
 * @param[in]  stream Name of event stream
 * @param[in]  fn     Callback when event occurs
 * @param[in]  arg    Argument to use with callback and handle
 * Note that arg is not a real handle.
 * @see subscription_add
 * @see subscription_each
 */
int
subscription_delete(clicon_handle     h,
		    char             *stream, 
		    subscription_fn_t fn,
		    void             *arg)
{
    struct backend_handle *cb = handle(h);
    struct handle_subscription   *hs;
    struct handle_subscription  **hs_prev;

    hs_prev = &cb->cb_subscription; /* this points to stack and is not real backpointer */
    for (hs = *hs_prev; hs; hs = hs->hs_next){
	/* XXX arg == hs->hs_arg */
	if (strcmp(hs->hs_stream, stream)==0 && hs->hs_fn == fn){
	    *hs_prev = hs->hs_next;
	    free(hs->hs_stream);
	    if (hs->hs_filter)
		free(hs->hs_filter);
	    if (hs->hs_arg)
		free(hs->hs_arg);
	    free(hs);
	    break;
	}
	hs_prev = &hs->hs_next;
    }
    return 0;
}

/*! Iterator over subscriptions
 *
 * NOTE: Never manipulate the child-list during operation or using the
 * same object recursively, the function uses an internal field to remember the
 * index used. It works as long as the same object is not iterated concurrently. 
 *
 * @param[in] h     clicon handle 
 * @param[in] hprev iterator, initialize with NULL
 * @code
 *   clicon_handle h;
 *   struct handle_subscription *hs = NULL;
 *   while ((hs = subscription_each(h, hs)) != NULL) {
 *     ...
 *   }
 * @endcode
 */
struct handle_subscription *
subscription_each(clicon_handle               h,
		  struct handle_subscription *hprev)
{
    struct backend_handle      *cb = handle(h);
    struct handle_subscription *hs = NULL;

    if (hprev)
	hs = hprev->hs_next;
    else
	hs = cb->cb_subscription;
    return hs;
}
