/*
 * CVS Version: $Id: config_commit.h,v 1.8 2013/08/05 14:19:55 olof Exp $
 *
 * Copyright 2009 RNR
 *
 * Originally written by Olof Hagsand
 */

#ifndef _CONFIG_COMMIT_H_
#define _CONFIG_COMMIT_H_

/*
 * Prototypes
 */ 

int from_client_validate(clicon_handle h, int s, struct clicon_msg *msg, const char *label);
int from_client_commit(clicon_handle h, int s, struct clicon_msg *msg, const char *label);
int candidate_commit(clicon_handle h, char *candidate, char *running);

#endif  /* _CONFIG_COMMIT_H_ */
