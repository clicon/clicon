/*
 *  CVS Version: $Id: xmlgen_xsl.h,v 1.3 2013/08/01 09:15:46 olof Exp $
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

 * XML XPATH and XSLT functions.
 */
#ifndef _XMLGEN_XSL_H
#define _XMLGEN_XSL_H

/*
 * Prototypes
 */
struct xml_node *xml_xpath(struct xml_node *xn_top, char *xpath);
struct xml_node *xpath_each(struct xml_node *xn_top, char *xpath, struct xml_node *prev);
struct xml_node **xpath_vec(struct xml_node *xn_top, char *xpath, int *xv_len);
struct xml_node *xml_xslt(struct xml_node *xslt, struct xml_node *xn);

#endif /* _XMLGEN_XSL_H */
