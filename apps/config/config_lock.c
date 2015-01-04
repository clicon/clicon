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
#include "config_lock.h"

/*
 * Easy way out: store an integer for candidate db which contains
 * the session-id of the client holding the lock.
 * more general: any database
 * we dont make any sanity check on who is locking.
 */
static int _db_locked = 0;

/*
 * db_lock
 */
int
db_lock(clicon_handle h, int id)
{
    _db_locked = id;
    clicon_debug(1, "%s: lock db by %u",  __FUNCTION__, id);
    return 0;
}

/*
 * db_unlock
 */
int
db_unlock(clicon_handle h)
{
    if (!_db_locked )
	return 0;
    _db_locked = 0;
    return 0;
}

/*
 * db_islocked
 * returns id of locker
 */
int
db_islocked(clicon_handle h)
{
    return _db_locked;
}

