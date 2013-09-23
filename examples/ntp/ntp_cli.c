/*
 *  CVS Version: $Id: ntp_cli.c,v 1.2 2013/08/09 13:25:26 olof Exp $
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
 * ntp clicon cli frontend
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

/* clicon */
#include <cligen/cligen.h>

#include <clicon/clicon.h>
#include <clicon/clicon_cli.h>

/*
 * Plugin initialization called when dynamically linked
 */ 
int
plugin_init(clicon_handle h)
{
    return 0;
}

/*
 * Called when this plugin is invoked and system is up
 */
int
plugin_start(clicon_handle h, int argc, char *argv[])
{
    return 0;
}

/*
 * Called when CLICON exits
 */
int
plugin_exit(clicon_handle h)
{
    return 0;
}

