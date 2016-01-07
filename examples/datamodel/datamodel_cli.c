/*
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
 * hello clicon cli frontend
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <fnmatch.h> /* matching strings */
#include <signal.h> /* matching strings */

/* clicon */
#include <cligen/cligen.h>
#include <clicon/clicon.h>
#include <clicon/clicon_cli.h>

/*
 * Plugin initialization
 */
int
plugin_init(clicon_handle h)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    srandom(tv.tv_usec);

    return 0;
}

int
test(clicon_handle h, cvec *vars, cg_var *arg)
{
    char        *dbname = clicon_candidate_db(h); /* name of database */
    cvec        **cvv; /* variable vector */
    char        *xpath;
    int          len;

    if ((xpath = cvec_find_str(vars, "xpath")) == NULL)
	goto done;
    if (clicon_dbget_xpath(h, dbname, NULL, xpath, &cvv, &len) < 0)
	goto done;
    if (len > 0)
	cvec_print(stdout, cvv[0]);
 done:
    return 0;

}
/*
Database for 
cnt {
    lst {
        id 44;
    }
    lst {
        id 34;
    }
}

cnt.lst.1
--------------------
        type: string len:   4 data: "!id"
        type: string len:   3 data: "44"

cnt.lst.n.98fbc42faedc02492397cb5962ea3a3ffc0a9243
--------------------
        type: number    len: 4  data: 1

cnt.lst.0
--------------------
        type: string len:   4 data: "!id"
        type: string len:   3 data: "34"

cnt.lst.n
--------------------
        type: number    len: 4  data: 2

cnt.lst.n.f1f836cb4ea6efb2a0b1b99f41ad8b103eff4b59
--------------------
        type: number    len: 4  data: 0
*/
