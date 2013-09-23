/*
 *  CVS Version: $Id: clicon_xml.h,v 1.12 2013/08/12 20:37:06 olof Exp $
 *
  Copyright (C) 2009-2013 Olof Hagsand and Benny Holmgren
  Olof Hagsand
 *
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
 * XML code
 */

#ifndef _CLICON_XML_H_
#define _CLICON_XML_H_

/*
 * lvmap_xml op codes
 */
enum {
    LVXML,       /* a.b{x=1} -> <a><b>1 */
    LVXML_VAL,       /* a.b{x=1} -> <a><b><x>1 */
    LVXML_VECVAL,   /* key: a.b.0{x=1} -> <a><b><x>1</x></b></a> och */
    LVXML_VECVAL2,  /* key: a.b.0{x=1} -> <a><x>1</x></a> och */
};


/* Forward declaration, proper definition in xmlgen_xml.h */
struct xml_node;

/*
 * Prototypes
 */
struct xml_node *db2xml(char *dbname, struct db_spec *db_spec, char *toptag);
int key2xml(char *key, char *dbname, struct db_spec *db_spec, struct xml_node *xtop);
int xml2db(struct xml_node *, struct db_spec *dbspec, char *dbname);

int save_db_to_xml(char *filename, struct db_spec *dbspec, char *dbname);
int load_xml_to_db(char *xmlfile, struct db_spec *dbspec, char *dbname);
int xml2txt(FILE *f, struct xml_node *x, int level);
int xml2cli(FILE *f, struct xml_node *x, char *prepend, enum genmodel_type gt, const char *label);

#endif  /* _CLICON_XML_H_ */
