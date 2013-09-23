/*
 *  CVS Version: $Id: clicon_lvmap.h,v 1.7 2013/08/01 09:15:46 olof Exp $
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
 * Lvalue map
 */

#ifndef _CLICON_LVMAP_H_
#define _CLICON_LVMAP_H_

#ifdef notanymore
/*
 * Config command exec types
 */
enum lvmap_op {
  LVEXEC_QUAGGA,
  LVEXEC_SNMP,
  LVEXEC_DNS,
  LVEXEC_INTERFACE,
  LVEXEC_DOT1Q,
  LVEXEC_AUTH,
  LVEXEC_HOSTNAME,
  LVEXEC_NTP,
  LVEXEC_LOGGING,
  LVEXEC_PROCFS,
};
#endif /* notanymore */

/*
 * lvmap_print op codes
 */
enum {
  LVPRINT_CMD,
  LVPRINT_CMD_ONCE,
  LVPRINT_COMMENT,
  LVPRINT_MODE,
  LVPRINT_MODE_ONCE,
  LVPRINT_MODE_CONDITIONAL,
  LVPRINT_MODE_ONCE_CONDITIONAL,
  LVPRINT_FUNC,
};
#define LVPRINT_ONCE(x)	(					\
	((x)==LVPRINT_CMD_ONCE) ||				\
	((x)==LVPRINT_MODE_ONCE) ||				\
	((x)==LVPRINT_MODE_ONCE_CONDITIONAL)			\
	)


#define LVPRINT_CONDITIONAL(x)	(					\
	((x)==LVPRINT_MODE_CONDITIONAL) ||				\
	((x)==LVPRINT_MODE_ONCE_CONDITIONAL)				\
	)

/*
 * LVPRINT_FUNC types
 */
enum {
  LVPRINT_FUNC_GRUB,
};

/*
 * Types
 */
/* Transform (mapping) from key to syntax using lvalue format strings */
struct lvmap{
    char *lm_key; /* Database key (can also be a [] key) */
    char *lm_fmt; /* Lvalue set format string */
    char *lm_neg; /* Lvalue negative format string */
    int   lm_op; /* Operation code, used for config execution */
    void *lm_opt; /* Optional argument - mapping dependent */
};

struct lvmap_dbkeyent{
    char *ld_dbkey;
    char *ld_matched;
    struct lvmap *ld_lm;
};

struct lm_iter{
    struct lvmap    *li_lm;
    int              li_vector; /* Set if key is vector */
    int              li_keyimax;
    int              li_i;
    int              li_regexi;
    int              li_regexn;
    char            *li_regexcnkgr;
    char           **li_regexkeys;
};

/*
 * Prototypes
 */ 
int lvmap_get_dbregex_sort (const void *p1, const void *p2);

int lvmap_get_dbregex(char *dbname, struct lvmap *lm, char *basekey,
		      const char *label, struct lvmap_dbkeyent **keys, 
		      int *nkeys, int addempty);

int lvmap_get_dbvector_sort (const void *p1, const void *p2);

int lvmap_get_dbvector(char *dbname, struct lvmap *lm, char *basekey,
		       const char *label, struct lvmap_dbkeyent **keys, 
		       int *nkeys, int addempty);

int lvmap_get_dbkeys(struct lvmap *lmap, char *dbname, char *keyprefix, 
		     const char *label, struct lvmap_dbkeyent **keys, int addempty);

int lvmap_print(FILE *f, char *dbname, struct lvmap *lmap, char *keyprefix);

char *lvmap_var_fmt (cvec *head, char *fmt);

char *lvmap_fmt (char *dbname, char *fmt, char *defkey);

int lvmap_match_key(struct lvmap *lmap, char *dbkey, struct lvmap **lmx);

#endif  /* _CLICON_LVMAP_H_ */
