/*
 *  CVS Version: $Id: netconf_filter.h,v 1.4 2013/08/01 09:15:46 olof Exp $
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
 *  netconf match & selection: get and edit operations
 *****************************************************************************/
#ifndef _NETCONF_FILTER_H_
#define _NETCONF_FILTER_H_

/*
 * Prototypes
 */ 
int xml_filter(struct xml_node *xf, struct xml_node *xn);
int netconf_xpath(struct xml_node *xsearch,
		  struct xml_node *xfilter, 
		  xf_t *xf, xf_t *xf_err, 
		  struct xml_node *xt);

#endif  /* _NETCONF_FILTER_H_ */
