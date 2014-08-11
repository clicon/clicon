/*

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

 */
#ifndef __CLICON_DB2TXT_H__
#define __CLICON_DB2TXT_H__

/*
 * Prototypes
 */
typedef char *(clicon_db2txtcb_t)(cg_var *);

/*
 * Parse a format file or buffer, return allocated output string
 */
char *clicon_db2txt(clicon_handle h, char *db, char *file);
char *clicon_db2txt_buf(clicon_handle h, char *db, char *buf);

#endif /* __CLICON_DB2TXT_H__ */
