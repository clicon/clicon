/*
 *  CVS Version: $Id: clicon_log.h,v 1.5 2013/08/01 09:15:46 olof Exp $
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
 * Regular logging and debugging. Syslog using levels.
 */

#ifndef _CLICON_LOG_H_
#define _CLICON_LOG_H_

/*
 * Constants
 */
#define DEBUG_RECORD 2

/*
 * Variables
 */
extern int debug;  

/*
 * Prototypes
 */
int clicon_log_init(char *ident, int upto, int err);
int clicon_log(int level, char *format, ...);
int clicon_debug_init(int dbglevel, int syslog);
int clicon_debug(int dbglevel, char *format, ...);
char *mon2name(int md);

#endif  /* _CLICON_LOG_H_ */
