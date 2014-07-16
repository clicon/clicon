/*
 *  CVS Version: $Id: clicon_spec.h,v 1.15 2013/09/20 11:45:08 olof Exp $
 *
  Copyright (C) 2009-2014 Olof Hagsand and Benny Holmgren
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

 */

#ifndef _CLICON_YANG_TYPE_H_
#define _CLICON_YANG_TYPE_H_

/*
 * Constants
 */
/*! Bit-fields used in options argument in yang_type_get()
 */
#define YANG_OPTIONS_LENGTH   0x01
#define YANG_OPTIONS_RANGE    YANG_OPTIONS_LENGTH
#define YANG_OPTIONS_PATTERN  0x02

/*
 * Types
 */


/*
 * Prototypes
 */
int        yang2cv_type(char *ytype, enum cv_type *cv_type);
char      *cv2yang_type(enum cv_type cv_type);
int        ys_cv_validate(cg_var *cv, yang_stmt *ys, char **reason);
int        yang_type_get(yang_stmt *ys, char **otype, char **rtype, 
			 int *options, int64_t *min, int64_t *max, char **pattern);

#endif  /* _CLICON_YANG_TYPE_H_ */
