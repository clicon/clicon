/*
 *  CVS Version: $Id: config_handle.h,v 1.2 2013/08/05 14:19:55 olof Exp $
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
 */

#ifndef _CONFIG_HANDLE_H_
#define _CONFIG_HANDLE_H_

/*
 * Prototypes 
 * not exported.
 */
/* backend handles */
clicon_handle backend_handle_init(void);

int backend_handle_exit(clicon_handle h);

dbdep_t *backend_dbdep(clicon_handle h); 

int backend_dbdep_set(clicon_handle h, dbdep_t *dbdep);

#endif  /* _CONFIG_HANDLE_H_ */
