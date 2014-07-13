/*
 *  CVS Version: $Id: clicon_netconf.h,v 1.6 2013/08/05 14:19:23 olof Exp $
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
 * The exported interface to plugins. External apps (eg frontend netconf plugins)
 * should only include this file (not the netconf_*.h)
 */

#ifndef _CLICON_NETCONF_H_
#define _CLICON_NETCONF_H_

/*
 * Types
 */
typedef int (*netconf_cb_t)(
    clicon_handle h, 
    struct xml_node *xorig, /* Original request. */
    struct xml_node *xn,    /* Sub-tree (under xorig) at child: <rpc><xn></rpc> */
    cbuf *xf,		    /* Output xml stream. For reply */
    cbuf *xf_err,	    /* Error xml stream. For error reply */
    void *arg               /* Argument given at netconf_register_callback() */
    );  

/*
 * Prototypes
 * (Duplicated. Also in netconf_*.h)
 */
int netconf_register_callback(clicon_handle h,
			      netconf_cb_t cb,   /* Callback called */
			      void *arg,       /* Arg to send to callback */
			      char *tag);      /* Xml tag when callback is made */
int netconf_create_rpc_error(cbuf *xf,            /* msg buffer */
			     struct xml_node *xr, /* orig request */
			     char *tag, 
			     char *type,
			     char *severity, 
			     char *message, 
			     char *info);

int
netconf_downcall(clicon_handle h, uint16_t op, char *plugin, char *func,
		 void *param, uint16_t paramlen, 
		 char **ret, uint16_t *retlen,
		 const void *label );


void netconf_ok_set(int ok);
int netconf_ok_get(void);

int netconf_xpath(struct xml_node *xsearch,
		  struct xml_node *xfilter, 
		   cbuf *xf, cbuf *xf_err, 
		  struct xml_node *xt);


#endif /* _CLICON_NETCONF_H_ */
