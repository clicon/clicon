/*
 *  CVS Version: $Id: netconf_rpc.h,v 1.7 2013/08/05 14:19:23 olof Exp $
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
 *  Code for handling netconf rpc messages
 *****************************************************************************/
#ifndef _NETCONF_RPC_H_
#define _NETCONF_RPC_H_

/*
 * Prototypes
 */ 
int 
netconf_rpc_dispatch(clicon_handle h,
		     struct db_spec *ds,
		     struct xml_node *xorig, 
		     struct xml_node *xn, 
		     xf_t *xf, 
		     xf_t *xf_err);

int netconf_create_rpc_reply(xf_t *xf,            /* msg buffer */
			     struct xml_node *xr, /* orig request */
			     char *body, int ok);
int netconf_create_rpc_error(xf_t *xf,            /* msg buffer */
			     struct xml_node *xr, /* orig request */
			     char *tag, 
			     char *type,
			     char *severity, 
			     char *message, 
			     char *info);

#endif  /* _NETCONF_RPC_H_ */
