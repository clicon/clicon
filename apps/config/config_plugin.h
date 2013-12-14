/*
  CVS Version: $Id: config_plugin.h,v 1.19 2013/08/31 06:32:46 benny Exp $ 

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

  */

#ifndef _CONFIG_PLUGIN_H_
#define _CONFIG_PLUGIN_H_

/*
 * Types
 */
/* Following are specific to backend. For common see clicon_plugin.h */
typedef int (plgreset_t)(void *h);		/* Reset system status */
typedef int (trans_begin_t)(void *h, char *);   /* Transaction begin hook */
typedef int (trans_complete_t)(void *h, char *);/* Transaction validation complete */
typedef int (trans_end_t)(void *h, char *);     /* Transaction succeed */
typedef int (trans_abort_t)(void *h, char *);   /* Transaction failed */

/*
 * Prototypes
 */
int  config_plugin_init(clicon_handle h);
int  plugin_initiate(clicon_handle h); 
void plugin_finish(clicon_handle h);

int  plugin_begin_hooks(clicon_handle h, char *candidate);
int  plugin_complete_hooks(clicon_handle h, char *candidate);
int  plugin_end_hooks(clicon_handle h, char *candidate);
int  plugin_abort_hooks(clicon_handle h, char *candidate);

int  plugin_reset_state(clicon_handle h);
int  plugin_start_hooks(clicon_handle h, int argc, char **argv);
int  plugin_downcall(clicon_handle h, struct clicon_msg_call_req *req,
		    uint16_t *retlen,  void **retarg);

#endif  /* _CONFIG_PLUGIN_H_ */
