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
 * ntp clicon netconf frontend
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>


/* clicon */
#include <cligen/cligen.h>
#include <clicon/clicon.h>
#include <clicon/clicon_backend.h>

/*
 * Commit callback. 
 * We do nothing here but simply create the config based on the current 
 * db once everything is done as if will then contain the new config.
 */
int
datamodel_commit(clicon_handle h, 
		 char *db,
		 trans_cb_type tt, 
		 lv_op_t op,
		 char *key,
		 void *arg)
{
    if (op == LV_SET)
	fprintf(stderr, "%s key:%s\n", __FUNCTION__, key);    
    return 0;
}


/*
 * Plugin initialization
 */
int
plugin_init(clicon_handle h)
{
    int retval = -1;

    if (dbdep(h, TRANS_CB_COMMIT, datamodel_commit, 
	      (void *)NULL, 3, "a[]", "a[].b", "a[].c") == NULL) {
	clicon_debug(1, "Failed to create dependency");
	goto done;
    }
    retval = 0;
  done:
    return retval;
}

