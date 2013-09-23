/*
 * CVS Version: $Id: config_client.h,v 1.6 2013/08/05 14:19:55 olof Exp $
 *
 * Copyright 2009 RNR
 *
 * Originally written by Olof Hagsand
 */

#ifndef _CONFIG_CLIENT_H_
#define _CONFIG_CLIENT_H_

/*
 * Types
 */ 
/*
 * Client entry.
 * Keep state about every connected client.
 */
struct client_entry{
    struct client_entry   *ce_next;  /* The clients linked list */
    struct sockaddr        ce_addr;  /* The clients (UNIX domain) address */
    int                    ce_s;     /* stream socket to client */
    int                    ce_nr;    /* Client number (for dbg/tracing) */
    int                    ce_stat_in; /* Nr of received msgs from client */
    int                    ce_stat_out;/* Nr of sent msgs to client */
    int                    ce_pid;   /* Process id */
    int                    ce_uid;   /* User id of calling process */
    clicon_handle          ce_handle; /* clicon config handle (all clients have same?) */
};

/* 
 * Exported variables 
 */
extern struct client_entry *ce_list;   /* The client list */

/*
 * Prototypes
 */ 
int from_client(int fd, void *arg);
struct client_entry *ce_add(struct client_entry **ce_list, 
			    struct sockaddr *addr);
int ce_rm(struct client_entry **ce_list, struct client_entry *ce);

#endif  /* _CONFIG_CLIENT_H_ */
