/*
  Copyright (C) 2009-2015 Olof Hagsand and Benny Holmgren

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


#ifndef _CONFIG_COMMIT_H_
#define _CONFIG_COMMIT_H_

/* Clicon internal, presented as void* to app's callback */
typedef struct {
  char *source_db;		/* Database 1 path */
  char *target_db;		/* Database 2 path */
  char *source_key;		/* Matched key 1 (matched part of key for tree deps */
  char *target_key;		/* Matched key 2 (matched part of key for tree deps */
  cvec *source_vec;		/* Cvec 1 */
  cvec *target_vec;		/* Cvec 2 */
  void *arg;		/* Application specific arg */
} commit_data_t;

/*
 * Prototypes
 */ 

int from_client_validate(clicon_handle h, int s, struct clicon_msg *msg, const char *label);
int from_client_commit(clicon_handle h, int s, struct clicon_msg *msg, const char *label);
int candidate_commit(clicon_handle h, char *candidate, char *running);

#endif  /* _CONFIG_COMMIT_H_ */
