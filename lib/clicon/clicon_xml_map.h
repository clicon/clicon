/*
 *
  Copyright (C) 2009-2015 Olof Hagsand and Benny Holmgren
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

#ifndef _CLICON_XML_MAP_H_
#define _CLICON_XML_MAP_H_

/*
 * lvmap_xml op codes
 */
enum {
    LVXML,       /* a.b{x=1} -> <a><b>1 */
    LVXML_VAL,       /* a.b{x=1} -> <a><b><x>1 */
    LVXML_VECVAL,   /* key: a.b.0{x=1} -> <a><b><x>1</x></b></a> och */
    LVXML_VECVAL2,  /* key: a.b.0{x=1} -> <a><x>1</x></a> och */
};


/*
 * Prototypes
 */
cxobj *db2xml_key(char *dbname, dbspec_key *dbspec, char *key_regex, char *toptag);
int key2xml(char *key, char *dbname, dbspec_key *db_spec, cxobj *xtop);
int xml2db(cxobj *, dbspec_key *dbspec, char *dbname);

int save_db_to_xml(char *filename, dbspec_key *dbspec, char *dbname);
int load_xml_to_db(char *xmlfile, dbspec_key *dbspec, char *dbname);
int xml2txt(FILE *f, cxobj *x, int level);
int xml2cli(FILE *f, cxobj *x, char *prepend, enum genmodel_type gt, const char *label);
int xml2json(FILE *f, cxobj *x, int level);
int xml_yang_validate(clicon_handle h, cxobj *xt, yang_spec *ys) ;
int xml2cvec(cxobj *xt, yang_stmt *ys, cvec **cvv0);
/* XXX temp backward */
#define xml2cvv xml2cvec

#endif  /* _CLICON_XML_MAP_H_ */
