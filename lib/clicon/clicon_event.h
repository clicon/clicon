/*
 *  CVS Version: $Id: clicon_event.h,v 1.3 2013/08/01 09:15:46 olof Exp $
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
 * Event handling and loop
 */

#ifndef _CLICON_EVENT_H_
#define _CLICON_EVENT_H_

/*
 * Prototypes
 */
int event_reg_fd(int fd, int (*fn)(int, void*), void *arg, char *str);

int event_unreg_fd(int s, int (*fn)(int, void*));

int event_reg_timeout(struct timeval t,  int (*fn)(int, void*), 
		      void *arg, char *str);

int event_unreg_timeout(int (*fn)(int, void*), void *arg);

int event_loop(void);

#endif  /* _CLICON_EVENT_H_ */
