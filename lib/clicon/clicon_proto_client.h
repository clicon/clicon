/*
 * CVS Version: $Id: clicon_proto_client.h,v 1.8 2013/08/01 09:15:46 olof Exp $
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
 * Client-side functions for clicon_proto protocol
 * Not actually a part of the clicon_proto lib, could be in the front-end lib
 * nectconf makes its own rpc code.
 * Therefore this file should probably be removed or moved to the frontend lib
 */

#ifndef _CLICON_PROTO_CLIENT_H_
#define _CLICON_PROTO_CLIENT_H_

int cli_proto_copy(char *spath, char *filename1, char *filename2);
int cli_proto_change(char *spath, char *db, lv_op_t op,
		     char *key, char *lvec, int lvec_len);
int cli_proto_commit(char *spath, char *running_db, char *db, 
		     int snapshot, int startup);
int cli_proto_validate(char *spath, char *db);
int cli_proto_save(char *spath, char *dbname, int snapshot, char *filename);
int cli_proto_load(char *spath, int replace, char *db, char *filename);
int cli_proto_initdb(char *spath, char *filename);
int cli_proto_rm(char *spath, char *filename);
int cli_proto_lock(char *spath, char *dbname);
int cli_proto_unlock(char *spath, char *dbname);
int cli_proto_kill(char *spath, int session_id);
int cli_proto_subscription(char *spath, char *stream, int *s);
int cli_proto_debug(char *spath, int level);

#endif  /* _CLICON_PROTO_CLIENT_H_ */
