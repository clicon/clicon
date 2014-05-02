/*
 *  CVS Version: $Id: ntp_backend.c,v 1.1 2013/08/08 14:21:45 olof Exp $
 *
  Copyright (C) 2009-2013 Olof Hagsand and Benny Holmgren

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
 * ntp clicon netconf frontend
 */


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>


/* clicon */
#include <cligen/cligen.h>
#include <clicon/clicon.h>
#include <clicon/clicon_backend.h>

#define NTPD		"ntpd"
#define NTPD_PARMS	"-g"

#define NTP_CONF	"/etc/ntp.conf"
#define NTP_DRIFT	"/osr/flash/etc/ntp.drift"
#if 0
#define NTP_RESTART	"/etc/rc.d/ntpd restart"
#define NTP_START	"/etc/rc.d/ntpd start"
#define NTP_STOP	"/etc/rc.d/ntpd stop"
#endif
#define NTP_LOGGING	"=syncall +clockall"

/* Flag set if config changed and we need to restart ntpd */
static int ntp_reload;

/* lvmap formats for db to config-file transforms */
struct lvmap ntp_conf_fmts[] = {
  {"ntp.server[]", "server\t\t$address\nrestrict\t$address", NULL, LVPRINT_CMD},
  {"ntp.logging", "logconfig\t" NTP_LOGGING "\n", NULL, LVPRINT_CMD},
  {NULL, NULL, NULL}
};


/*
 * Commit callback. 
 * We do nothing here but simply create the config based on the current 
 * db once everything is done as if will then contain the new config.
 */
int
ntp_commit(clicon_handle h, 
	   char *db,
	   trans_cb_type tt, 
	    lv_op_t op,
	    char *key,
	    void *arg)
{
    fprintf(stderr, "%s\n", __FUNCTION__);    
    ntp_reload = 1; /* Mark NTP config as changed */
    return 0;
}


/*
 * Plugin initialization
 */
int
plugin_init(clicon_handle h)
{
    int i;
    char *key;
    int retval = -1;

    for (i = 0; ntp_conf_fmts[i].lm_key; i++) {
	key = ntp_conf_fmts[i].lm_key;
	if (dbdep(h, TRANS_CB_COMMIT, ntp_commit, (void *)NULL, 1, key) == NULL) {
	    clicon_debug(1, "Failed to create dependency '%s'", key);
	    goto done;
	}
	clicon_debug(1, "Created dependency '%s'", key);
    }
    retval = 0;
  done:
    return retval;
}

/*
 * Plugin start
 */
int
plugin_start(clicon_handle h, int argc, char **argv)
{
    return 0;
}

/*
 * Reset reload-flag before we start.
 */
int
transaction_begin(clicon_handle h)
{
    fprintf(stderr, "%s\n", __FUNCTION__);    
    ntp_reload = 0;
    return 0;
}

/*
 * If commit is successful the current db has been replaced by the committed 
 * candidate. We simply re-create a new NTP_CONF based on it.
 */
int
transaction_end(clicon_handle h)
{
    int retval = -1;
    FILE *out = NULL;

    fprintf(stderr, "%s\n", __FUNCTION__);    
    if (ntp_reload == 0)
	return 0; /* Nothing has changed */

    if ((out = fopen(NTP_CONF, "w")) == NULL) {
	clicon_err(OE_CFG, errno, "%s: fopen: %s", __FUNCTION__, strerror(errno));
	goto catch;
    }
    
    fprintf (out, "driftfile\t%s\n", NTP_DRIFT);
    fprintf (out, "restrict\t127.0.0.1\n");
    lvmap_print(out, clicon_running_db(h), ntp_conf_fmts, NULL);
    fclose(out);

    /* Restart ntpd. XXX Any way to do a soft reconfig? */
    clicon_debug(1, "Re-loading NTP daemon\n");
    { /* XXX: platform dependent */
	char buf[512];
	snprintf (buf, sizeof (buf)-1, "pkill -%d %s", SIGTERM, NTPD);

	if (clicon_proc_run (buf, NULL, 0) > 0)
	    sleep(1); /* Wait for the process to die. */
    }
    retval = clicon_proc_daemon ("/usr/bin/" NTPD " " NTPD_PARMS);
    
catch:
    unchunk_group(__FUNCTION__);
    return retval;
}
