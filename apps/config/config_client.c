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
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include <clicon/clicon.h>

#include "clicon_backend_api.h"
#include "config_commit.h"
#include "config_lib.h"
#include "config_lock.h"
#include "config_plugin.h"
#include "config_client.h"
#include "config_dbdiff.h"
#include "config_dbdep.h"
#include "config_handle.h"

/*
 * See also backend_notify
 */
static struct subscription *
subscription_add(struct client_entry *ce, char *stream)
{
    struct subscription *su = NULL;

    if ((su = malloc(sizeof(*su))) == NULL){
	clicon_err(OE_PLUGIN, errno, "malloc");
	goto done;
    }
    memset(su, 0, sizeof(*su));
    su->su_stream = strdup(stream);
    su->su_next = ce->ce_subscription;
    ce->ce_subscription = su;
  done:
    return su;
}

static struct client_entry *
ce_find_bypid(struct client_entry *ce_list, int pid)
{
    struct client_entry *ce;

    for (ce = ce_list; ce; ce = ce->ce_next)
	if (ce->ce_pid == pid)
	    return ce;
    return NULL;
}

static int
subscription_delete(struct subscription *su)
{
    free(su->su_stream);
    free(su);
    return 0;
}

/*! Remove client entry state
 * Close down everything wrt clients (eg sockets, subscriptions)
 * Finally actually remove client struct in handle
 * See also backend_client_delete()
 */
int
backend_client_rm(clicon_handle h, struct client_entry *ce)
{
    struct client_entry   *c;
    struct client_entry   *c0;
    struct client_entry  **ce_prev;
    struct subscription   *su;

    c0 = backend_client_list(h);
    ce_prev = &c0; /* this points to stack and is not real backpointer */
    for (c = *ce_prev; c; c = c->ce_next){
	if (c == ce){
	    if (ce->ce_s){
		event_unreg_fd(ce->ce_s, from_client);
		close(ce->ce_s);
		ce->ce_s = 0;
	    }
	    while ((su = ce->ce_subscription) != NULL){
		ce->ce_subscription = su->su_next;
		subscription_delete(su);
	    }
	    break;
	}
	ce_prev = &c->ce_next;
    }
    return backend_client_delete(h, ce); /* actually purge it */
}

/*
 * Change entry set/delete in database
 */
static int
from_client_change(clicon_handle h,
		   int s, 
		   int pid, 
		   struct clicon_msg *msg, 
		   const char *label)
{
    int         retval = -1;
    uint32_t    lvec_len;
    char       *lvec;
    char       *basekey;
    char       *dbname;
    lv_op_t     op;
    cvec       *vr = NULL;
    char       *str = NULL;
    dbspec_key *dbspec;
    char       *candidate_db;

    dbspec = clicon_dbspec_key(h);
    if (clicon_msg_change_decode(msg, &dbname, &op,
				&basekey, 
				&lvec, &lvec_len, 
				label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    if ((candidate_db = clicon_candidate_db(h)) == NULL){
	send_msg_err(s, 0, 0, "candidate db not set");
	goto done;
    }
    /* candidate is locked by other client */
    if (strcmp(dbname, candidate_db) == 0 &&
	db_islocked(h) &&
	pid != db_islocked(h)){
	send_msg_err(s, OE_DB, 0,
		     "lock failed: locked by %d", db_islocked(h));
	goto done;
    }

    /* Update database */
    if((vr = lvec2cvec (lvec, lvec_len)) == NULL)
	goto done;
    if (db_lv_op_exec(dbspec, dbname, basekey, op, vr) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
//	send_msg_err(s, OE_DB, 0, "Executing operation on %s", dbname);
	goto done;
    }
    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    if (str)
	free(str);
    if (vr)
	cvec_free (vr);
    return retval;
}

/*
 * Dump database to file
 */
static int
from_client_save(clicon_handle h,
		 int s, 
		 struct clicon_msg *msg, 
		 const char *label)
{
    char *filename;
    int retval = -1;
    char *db;
    uint32_t snapshot;

    if (clicon_msg_save_decode(msg, 
			      &db, 
			      &snapshot,
			      &filename,
			      label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    if (snapshot){
	if (config_snapshot(clicon_dbspec_key(h), db, clicon_archive_dir(h)) < 0){
	    send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);

	    goto done;
	}
    }
    else
	if (save_db_to_xml(filename, clicon_dbspec_key(h), db) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	    goto done;
	}
    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * Load file into database
 */
static int
from_client_load(clicon_handle h,
		 int s, 
		 int pid, 
		 struct clicon_msg *msg,
		 const char *label)

{
    char *filename = NULL;
    int   retval = -1;
    char *dbname = NULL;
    int   replace = 0;
    char *candidate_db;

    if (clicon_msg_load_decode(msg, 
			       &replace,
			       &dbname, 
			       &filename,
			       label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    if ((candidate_db = clicon_candidate_db(h)) == NULL){
	send_msg_err(s, 0, 0, "candidate db not set");
	goto done;
    }
    /* candidate is locked by other client */
    if (strcmp(dbname, candidate_db) == 0 &&
	db_islocked(h) &&
	pid != db_islocked(h)){
	send_msg_err(s, OE_DB, 0, "lock failed: locked by %d", db_islocked(h));
	goto done;
    }
    if (replace){
	if (unlink(dbname) < 0){
	    send_msg_err(s, OE_UNIX, 0, "rm %s %s", filename, strerror(errno));
	    goto done;
	}
	if (db_init(dbname) < 0) 
	    goto done;
    }

    if (load_xml_to_db(filename, clicon_dbspec_key(h), dbname) < 0) {
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	return -1;
    }

    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * Initialize database 
 */
static int
from_client_initdb(clicon_handle h,
		   int s, 
		   int pid, 
		   struct clicon_msg *msg, 
		   const char *label)
{
    char  *filename1;
    int    retval = -1;
    char  *candidate_db;

    if (clicon_msg_initdb_decode(msg, 
			      &filename1,
			      label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    if ((candidate_db = clicon_candidate_db(h)) == NULL){
	send_msg_err(s, 0, 0, "candidate db not set");
	goto done;
    }
    /* candidate is locked by other client */
    if (strcmp(filename1, candidate_db) == 0 &&
	db_islocked(h) &&
	pid != db_islocked(h)){
	send_msg_err(s, OE_DB, 0, "lock failed: locked by %d", db_islocked(h));
	goto done;
    }

    if (db_init(filename1) < 0) 
	goto done;
    /* Change mode if shared candidate. XXXX full rights for all is no good */
    if (strcmp(filename1, candidate_db) == 0)
	chmod(filename1, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * Remove file
 * XXX: restrict to app-dir, or even $appdir/DB?
 */
static int
from_client_rm(clicon_handle h,
	       int s, 
	       int pid, 
	       struct clicon_msg *msg, 
	       const char *label)
{
    char *filename1;
    int   retval = -1;
    char *candidate_db;

    if (clicon_msg_rm_decode(msg, 
			      &filename1,
			      label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    if ((candidate_db = clicon_candidate_db(h)) == NULL){
	send_msg_err(s, 0, 0, "candidate db not set");
	goto done;
    }
    /* candidate is locked by other client */
    if (strcmp(filename1, candidate_db) == 0 &&
	db_islocked(h) &&
	pid != db_islocked(h)){
	send_msg_err(s, OE_DB, 0, "lock failed: locked by %d", db_islocked(h));
	goto done;
    }

    if (unlink(filename1) < 0){
	send_msg_err(s, OE_UNIX, 0, "rm %s %s", filename1, strerror(errno));
	goto done;
    }
    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * Copy file from file1 to file2
 */
static int
from_client_copy(clicon_handle h,
		 int s, 
		 int pid, 
		 struct clicon_msg *msg, 
		 const char *label)
{
    char *filename1;
    char *filename2;
    int   retval = -1;
    char *candidate_db;

    if (clicon_msg_copy_decode(msg, 
			      &filename1,
			      &filename2,
			      label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    if ((candidate_db = clicon_candidate_db(h)) == NULL){
	send_msg_err(s, 0, 0, "candidate db not set");
	goto done;
    }

    /* candidate is locked by other client */
    if (strcmp(filename2, candidate_db) == 0 &&
	db_islocked(h) &&
	pid != db_islocked(h)){
	send_msg_err(s, OE_DB, 0, "lock failed: locked by %d", db_islocked(h));
	goto done;
    }

    if (file_cp(filename1, filename2) < 0){
	send_msg_err(s, OE_UNIX, errno, "copy %s to %s", filename1, filename2);
	goto done;
    }
    /* Change mode if shared candidate. XXXX full rights for all is no good */
    if (strcmp(filename2, candidate_db) == 0)
	chmod(filename2, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * Lock db
 */
static int
from_client_lock(clicon_handle h,
		 int s, 
		 int pid, 
		 struct clicon_msg *msg, 
		 const char *label)
{
    char *db;
    int   retval = -1;
    char *candidate_db;


    if (clicon_msg_lock_decode(msg, 
			      &db,
			      label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    if ((candidate_db = clicon_candidate_db(h)) == NULL){
	send_msg_err(s, 0, 0, "candidate db not set");
	goto done;
    }
    if (strcmp(db, candidate_db)){
	send_msg_err(s, OE_DB, 0, "can not lock %s, only %s", 
		     db, candidate_db);
	goto done;
    }
    if (db_islocked(h)){
	if (pid == db_islocked(h))
	    ;
	else{
	    send_msg_err(s, OE_DB, 0, "lock failed: locked by %d", db_islocked(h));
	    goto done;
	}
    }
    else
	db_lock(h, pid);
    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * unlock db
 */
static int
from_client_unlock(clicon_handle h,
		   int s, 
		   int pid, 
		   struct clicon_msg *msg, 
		   const char *label)
{
    char *db;
    int   retval = -1;
    char *candidate_db;


    if (clicon_msg_unlock_decode(msg, 
			      &db,
			      label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    if ((candidate_db = clicon_candidate_db(h)) == NULL){
	send_msg_err(s, 0, 0, "candidate db not set");
	goto done;
    }

    if (strcmp(db, candidate_db)){
	send_msg_err(s, OE_DB, 0, "can not unlock %s, only %s", 
		     db, clicon_candidate_db(h));
	goto done;
    }
    if (db_islocked(h)){
	if (pid == db_islocked(h))
	    db_unlock(h);
	else{
	    send_msg_err(s, OE_DB, 0, "unlock failed: locked by %d", db_islocked(h));
	    goto done;
	}
    }
    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * Kill session
 * Kill the process
 */
static int
from_client_kill(clicon_handle h,
		 int s, 
		 struct clicon_msg *msg, 
		 const char *label)
{
    uint32_t pid; /* other pid */
    int retval = -1;
    struct client_entry *ce;

    if (clicon_msg_kill_decode(msg, 
			      &pid,
			      label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    /* may or may not be in active client list, probably not */
    if ((ce = ce_find_bypid(backend_client_list(h), pid)) != NULL)
	backend_client_rm(h, ce);
    if (kill (pid, 0) != 0 && errno == ESRCH) /* Nothing there */
	;
    else{
	killpg(pid, SIGTERM);
	kill(pid, SIGTERM);
#if 0 /* Hate sleeps we assume it died, see also 0 in next if.. */
	sleep(1);
#endif
    }
    if (1 || (kill (pid, 0) != 0 && errno == ESRCH)){ /* Nothing there */
	/* clear from locks */
	if (db_islocked(h) == pid)
	    db_unlock(h);
    }
    else{ /* failed to kill client */
	send_msg_err(s, OE_DB, 0, "failed to kill %d", pid);
	goto done;
    }
    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * from_client_debug
 * Set debug level. This is global, not just for the session.
 */
static int
from_client_debug(clicon_handle h,
		 int s, 
		 struct clicon_msg *msg, 
		 const char *label)
{
    int retval = -1;
    uint32_t level;

    if (clicon_msg_debug_decode(msg, 
				&level,
				label) < 0){
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    clicon_debug_init(level, NULL); /* 0: dont debug, 1:debug */

    if (send_msg_ok(s) < 0)
	goto done;
    retval = 0;
  done:
    return retval;
}

/*
 * Call backend plugin
 */
static int
from_client_call(clicon_handle h,
		 int s, 
		 struct clicon_msg *msg, 
		 const char *label)
{
    int retval = -1;
    void *reply_data = NULL;
    uint16_t reply_data_len = 0;
    struct clicon_msg_call_req *req;

    if (clicon_msg_call_decode(msg, &req, label) < 0) {
	send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
#ifdef notyet
    if (!strlen(req->cr_plugin)) /* internal */
	internal_function(req, &reply_data_len, &reply_data);
    else
#endif
	if (plugin_downcall(h, req, &reply_data_len, &reply_data) < 0)  {
	    send_msg_err(s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	    goto done;
	}
    
    retval = send_msg_reply(s,CLICON_MSG_OK, (char *)reply_data, reply_data_len);
    free(reply_data);

 done:
    return retval;
}


/*
 * create subscription for notifications
 */
static int
from_client_subscription(clicon_handle h,
			 struct client_entry *ce,
			 struct clicon_msg *msg, 
			 const char *label)
{
    char *stream;
    int retval = -1;
    struct subscription *su;
    clicon_log_notify_t *old;

    if (clicon_msg_subscription_decode(msg, 
				       &stream,
				       label) < 0){
	send_msg_err(ce->ce_s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }

    if ((su = subscription_add(ce, stream)) == NULL){
	send_msg_err(ce->ce_s, clicon_errno, clicon_suberrno,
		     clicon_err_reason);
	goto done;
    }
    /* Avoid recursion when sending logs */
    old = clicon_log_register_callback(NULL, NULL);
    if (send_msg_ok(ce->ce_s) < 0)
	goto done;
    clicon_log_register_callback(old, NULL); /* XXX NULL arg? */
    retval = 0;
  done:
    return retval;
}


/*! A message has arrived from a client
 */
int
from_client(int s, void* arg)
{
    struct client_entry *ce = (struct client_entry *)arg;
    clicon_handle h = ce->ce_handle;
    struct clicon_msg *msg;
//    int retval = -1;
    int eof;

    assert(s == ce->ce_s);
    if (clicon_msg_rcv(ce->ce_s, &msg, &eof, __FUNCTION__) < 0)
	goto done;
    if (eof){ 
	backend_client_rm(h, ce); 
	goto done;
    }
    switch (msg->op_type){
    case CLICON_MSG_COMMIT:
	if (from_client_commit(h, ce->ce_s, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_VALIDATE:
	if (from_client_validate(h, ce->ce_s, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_CHANGE:
	if (from_client_change(h, ce->ce_s, ce->ce_pid, msg, 
			    (char *)__FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_SAVE:
	if (from_client_save(h, ce->ce_s, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_LOAD:
	if (from_client_load(h, ce->ce_s, ce->ce_pid, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_RM:
	if (from_client_rm(h, ce->ce_s, ce->ce_pid, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_INITDB:
	if (from_client_initdb(h, ce->ce_s, ce->ce_pid, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_COPY:
	if (from_client_copy(h, ce->ce_s, ce->ce_pid, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_LOCK:
	if (from_client_lock(h, ce->ce_s, ce->ce_pid, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_UNLOCK:
	if (from_client_unlock(h, ce->ce_s, ce->ce_pid, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_KILL:
	if (from_client_kill(h, ce->ce_s, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_DEBUG:
	if (from_client_debug(h, ce->ce_s, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_CALL:
	if (from_client_call(h, ce->ce_s, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    case CLICON_MSG_SUBSCRIPTION:
	if (from_client_subscription(h, ce, msg, __FUNCTION__) < 0)
	    goto done;
	break;
    default:
	send_msg_err(s, OE_PROTO, 0, "Unexpected message: %d", msg->op_type);
	goto done;
    }
//    retval = 0;
  done:
    unchunk_group(__FUNCTION__);
//    return retval;
    return 0; // -1 here terminates
}

