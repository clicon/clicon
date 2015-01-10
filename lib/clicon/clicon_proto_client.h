/*
 *
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

 *
 * Client-side functions for clicon_proto protocol
 * Historically this code was part of the clicon_cli application. But
 * it should (is?) be general enough to be used by other applications.
 */

#ifndef _CLICON_PROTO_CLIENT_H_
#define _CLICON_PROTO_CLIENT_H_

int clicon_proto_copy(char *spath, char *filename1, char *filename2);
int clicon_proto_change(char *spath, char *db, lv_op_t op,
		     char *key, char *lvec, int lvec_len);
int clicon_proto_commit(char *spath, char *running_db, char *db, 
		     int snapshot, int startup);
int clicon_proto_validate(char *spath, char *db);
int clicon_proto_save(char *spath, char *dbname, int snapshot, char *filename);
int clicon_proto_load(char *spath, int replace, char *db, char *filename);
int clicon_proto_initdb(char *spath, char *filename);
int clicon_proto_rm(char *spath, char *filename);
int clicon_proto_lock(char *spath, char *dbname);
int clicon_proto_unlock(char *spath, char *dbname);
int clicon_proto_kill(char *spath, int session_id);
int clicon_proto_subscription(char *spath, int status, char *stream, int *s);
int clicon_proto_debug(char *spath, int level);

/*
 * Backward compatible functions
 */
#if 0
#define cli_proto_copy     clicon_proto_copy
#define cli_proto_change   clicon_proto_change
#define cli_proto_commit   clicon_proto_commit
#define cli_proto_validate clicon_proto_validate
#define cli_proto_save     clicon_proto_save
#define cli_proto_load     clicon_proto_load
#define cli_proto_initdb   clicon_proto_initdb
#define cli_proto_rm       clicon_proto_rm
#define cli_proto_lock     clicon_proto_lock
#define cli_proto_unlock   clicon_proto_unlock
#define cli_proto_kill     clicon_proto_kill
#define cli_proto_subscription clicon_proto_subscription
#define cli_proto_debug    clicon_proto_debug
#endif

#endif  /* _CLICON_PROTO_CLIENT_H_ */
