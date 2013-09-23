/*
 *  CVS Version: $Id: clicon_backend_api.h,v 1.2 2013/08/05 14:19:55 olof Exp $
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
 * Part of the external API to plugins. Applications should not include
 * this file directly (only via clicon_backend.h).
 */

#ifndef _CLICON_BACKEND_API_H_
#define _CLICON_BACKEND_API_H_

/*
 * Types
 */
enum trans_cb_type {/* Note, this is a bitmask, BOTH is bitwise combination */
    TRANS_CB_VALIDATE=0x1,
    TRANS_CB_COMMIT=0x2,
    TRANS_CB_BOTH=0x3,
};
typedef enum trans_cb_type trans_cb_type;

/*
 * Transaction operation (commit/validate after object add/del/modify) to backend plugin 
 * when dependencies changed
 */
typedef int (*trans_cb)(clicon_handle h, 
			  char *dbname, 
			  trans_cb_type trans_cb_type, 
			  lv_op_t op, 
			  char *key, 
			  void *arg);  

/*
 * Generic downcall registration. Enables any function to be called from (cli) frontend
 * to backend. Like an RPC on application-level.
 */
typedef int (*downcall_cb)(clicon_handle h, uint16_t op, uint16_t len, void *arg, 
			   uint16_t *retlen, void **retarg);



/*
 * DB dependency handle pointer
 */
typedef void *dbdep_handle_t;

/*
 * Add DB dependency
 */
dbdep_handle_t dbdep(clicon_handle h, trans_cb_type, trans_cb, void *, int, ...);

/*
 * Add DB dependency entry
 */
int dbdep_ent(dbdep_handle_t, const char *, const char *);

#endif /* _CLICON_BACKEND_API_H_ */
