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

    /* Set active syntax mode */
//    if (!cli_set_syntax_mode(h, "operation"))
//	cli_output(stderr, "Failed to set syntax\n");

    return 0;
}

int
plugin_start(clicon_handle h, int argc, char *argv[])
{
    char *str;
    char *mode = NULL;

    if (argc < 2)
	return 0; /* No extra command-line args */
    argc--; argv++;
    str = clicon_strjoin(argc, argv, " ", __FUNCTION__);
    cli_exec(h, str, &mode, NULL);
    unchunk_group(__FUNCTION__);
    cli_set_exiting(h, 1); /* quit after executing command - one-liner */
    return 0;
}


