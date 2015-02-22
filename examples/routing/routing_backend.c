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
 * IETF yang routing example
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
routing_commit(clicon_handle h, 
	       commit_op op, 
	       commit_data d)
{
    if (op == CO_ADD)
        fprintf(stderr, "%s key:%s\n", __FUNCTION__, commit_key2(d));
    return 0;
}


int
routing_validate(clicon_handle h, 
		 commit_op op, 
		 commit_data d)
{
    if (op == CO_ADD)
        fprintf(stderr, "%s key:%s\n", __FUNCTION__, commit_key2(d));
    return 0;
}


/*
 * Plugin initialization
 */
int
plugin_init(clicon_handle h)
{
    int retval = -1;

    if (dbdep_tree(h, 0, routing_commit, 
			 (void *)NULL, "a[]*") == NULL) {
	clicon_debug(1, "Failed to create dependency");
	goto done;
    }
    if (dbdep_tree_validate(h, 0, routing_validate, 
			 (void *)NULL, "a[]*") == NULL) {
	clicon_debug(1, "Failed to create dependency");
	goto done;
    }
    retval = 0;
  done:
    return retval;
}

