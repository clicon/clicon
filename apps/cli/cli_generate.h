/*
 * CVS Version: $Id: cli_generate.h,v 1.7 2013/09/05 20:13:36 olof Exp $
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

 */

#ifndef _CLI_GENERATE_H_
#define _CLI_GENERATE_H_

/*
 * Prototypes
 */
int dbspec2cli(clicon_handle h, parse_tree *pt0, parse_tree *pt1, enum genmodel_type gt);

#endif  /* _CLI_GENERATE_H_ */
