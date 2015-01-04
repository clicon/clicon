/*

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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <cligen/cligen.h>
#include <clicon/clicon.h>
#include <clicon/clicon_netconf.h>


static int 
netconf_config_cb(clicon_handle h, 
		  cxobj *xorig, /* Original request. */
		  cxobj *xn,    /* Sub-tree (under xorig) at child of <rpc> */
		  cbuf *xf,		    /* Output xml stream. For reply */
		  cbuf *xf_err,	    /* Error xml stream. For error reply */
		  void *arg)
{
    fprintf(stderr, "%s\n", __FUNCTION__);
    return 0;
}

/*
 * Plugin initialization
 */
int
plugin_init(clicon_handle h)
{
    fprintf(stderr, "%s\n", __FUNCTION__);
    netconf_register_callback(h,
			      netconf_config_cb,  /* Callback called */
			      NULL,               /* Arg to send to callback */
			      "config");         /* Xml tag when callback is made */

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

int
plugin_exit(clicon_handle h)
{
    return 0;
}


