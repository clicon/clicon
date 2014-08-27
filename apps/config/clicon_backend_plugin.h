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
 * Part of clicon_backend API
 * Contains prototypes for plugins that must or may exist in a backend/config plugin.
 */
#ifndef _CLICON_BACKEND_PLUGIN_H_
#define _CLICON_BACKEND_PLUGIN_H_

/* Following are specific to backend. For common plugins see clicon_plugin.h.
    Note that the following should match the types in config_plugin.h
 */

/* Reset system state to original state. Eg at reboot before running thru config. */
int plugin_reset(clicon_handle h);

/* Called before a commit/validate sequence begins. Eg setup state before commit */
int transaction_begin(clicon_handle h);

/* Called after a validation completed succesfully (but before commit). */
int transaction_complete(clicon_handle h);

/* Called after a commit sequence completed succesfully. */
int transaction_end(clicon_handle h);

/* Called if commit or validate sequence fails. After eventual rollback. */
int transaction_abort(clicon_handle h);

#endif /* _CLICON_BACKEND_PLUGIN_H_ */
