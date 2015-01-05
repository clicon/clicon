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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <openssl/sha.h>

/* clicon */
#include <cligen/cligen.h>
#include <clicon/clicon.h>
#include <clicon/clicon_backend.h>


/* Opcodes; to avoid casting of void* pointer to integer */
typedef struct {
    char *key; 
    char *arg;
} keyops_t;

keyops_t key2ops[] = {
    {"hello.vector[]",     NULL},	
    {"hello",     NULL},	
    {NULL, 0}
};


int hello_validate(clicon_handle, char *,trans_cb_type, lv_op_t, char *, void *);
int hello_commit(clicon_handle, char *, trans_cb_type, lv_op_t, char *, void *);

/*
 * Plugin initialization
 */
int
plugin_init(clicon_handle h)
{
    int             i;
    char           *key;
    int             retval = -1;
    dbdep_handle_t *dp;

    for (i = 0; key2ops[i].key; i++) {
	key = key2ops[i].key;
	if ((dp = dbdep(h, 0, TRANS_CB_VALIDATE, hello_validate, 
			(void*)&key2ops[i].arg, key)) == NULL) {
	    clicon_debug(1, "Failed to create dependency '%s'", key);
	    goto done;
	}
	if ((dp = dbdep(h, 0, TRANS_CB_COMMIT, hello_commit, 
			(void*)&key2ops[i].arg, key)) == NULL) {
	    clicon_debug(1, "Failed to create dependency '%s'", key);
	    goto done;
	}
	clicon_debug(2, "%s: Created dependency '%s'", __FUNCTION__, key);
    }

    retval = 0;
  done:
    return retval;
}

int
plugin_exit(clicon_handle h)
{
    return 0;
}

/*
 * Plugin start
 * Called once everything has been initialized, right before
 * the main event loop is entered.
 */
int
plugin_start(clicon_handle h, int argc, char **argv)
{
    return 0;
}

/*
 * Plugin reset
 * The system 'state' should be the same as the contents of running_db
 * This is called on start if you call clicon_config with -R
 */
int
plugin_reset(clicon_handle h, char *dbname)
{
    return 0;
}

/*
 * Reset reload-flag before we start.
 */
int
transaction_begin(clicon_handle h)
{
    return 0;
}

/*
 * Validation complete
 */
int
transaction_complete(clicon_handle h)
{

    return 0;
}

/*
 * If commit is successful the current db has been replaced by the committed 
 * candidate. 
 */
int
transaction_end(clicon_handle h)
{
    int retval = -1;

    retval = 0;
    unchunk_group(__FUNCTION__);
    return retval;
}


/*
 * 
 */
int
transaction_abort(clicon_handle h)
{
    return 0;
}


/*
 * hello_validate
 *  Check that all fields are valid. 
 */
int
hello_validate(clicon_handle h, 
		 char *dbname,
		 trans_cb_type type, 
		 lv_op_t op,
		 char *key,
		 void *arg)
{
    dbspec_key     *dbspec;
    int                 retval = -1;

    clicon_debug(1, "%s %s\n", __FUNCTION__, key);
    clicon_debug(1, "  dbname:  %s\n", dbname);
    clicon_debug(1, "  key: %s\n", key);
    clicon_debug(1, "  op:  %s\n", op==LV_DELETE?"delete":op==LV_SET?"set":"merge");
    if (op !=  LV_SET && op != LV_MERGE)
	return 0;
    dbspec = clicon_dbspec_key(h);
    if (key2spec_key(dbspec, key) == NULL){
	clicon_log(LOG_ERR, "%s: %s not found in database spec\n",
		   __FUNCTION__, key);
	goto done;
    }
    retval = 0;
  done:
    return retval;
}


/*
 * Commit callback. 
 * Assume a validated object.
 */
int
hello_commit(clicon_handle h, 
	       char *dbname,
	       trans_cb_type type, 
	       lv_op_t op,
	       char *key,
	       void *arg)
{
    int                 retval = -1;

    clicon_debug(1, "%s %s\n", __FUNCTION__, key);
    clicon_debug(1, "  dbname:  %s\n", dbname);
    clicon_debug(1, "  key: %s\n", key);
    clicon_debug(1, "  op:  %s\n", op==LV_DELETE?"delete":op==LV_SET?"set":"merge");
    switch (op){
    case LV_DELETE:
	break;
    case LV_SET: 
	break;
    case LV_MERGE:
	break;
    }
    retval = 0;

    return retval;
}


/*
 * A "Down-call" function. Return a string
 */
int
hello_command(clicon_handle h, uint16_t op, uint16_t len, void *arg, 
	      uint16_t *reply_data_len, void **reply_data)
{
    char *str = (char*)arg; /* with length len */
    char *ret;

    fprintf(stderr, "%s: op:%d str:%s\n", __FUNCTION__, op, str);

    if ((ret = strdup("return value")) == NULL){
	clicon_err(OE_UNIX, errno , "strdup");
	return -1;
    }

    *reply_data = (void *)ret;
    *reply_data_len = strlen(ret)+1;

    return 0;
}


