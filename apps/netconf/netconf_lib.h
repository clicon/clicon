/*
 *  CVS Version: $Id: netconf_lib.h,v 1.9 2013/08/05 14:19:23 olof Exp $
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

 *
 *  Netconf lib
 *****************************************************************************/
#ifndef _NETCONF_LIB_H_
#define _NETCONF_LIB_H_

/*
 * Types
 */ 
enum target_type{ /* netconf */
    RUNNING,
    CANDIDATE
}; 
enum transport_type{ 
    NETCONF_SSH,  /* RFC 4742 */
    NETCONF_SOAP,  /* RFC 4743 */
};

enum operation_type{ /* edit-config */
    OP_MERGE,  /* merge config-data */
    OP_REPLACE,/* replace or create config-data */
    OP_CREATE, /* create config data, error if exist */
    OP_DELETE, /* delete config data, error if it does not exist */
    OP_REMOVE, /* delete config data */
    OP_NONE
};

enum test_option{ /* edit-config */
    SET,
    TEST_THEN_SET,
    TEST_ONLY
};

enum error_option{ /* edit-config */
    STOP_ON_ERROR,
    CONTINUE_ON_ERROR
};

/*
 * Variables
 */ 
extern enum transport_type transport;
extern int cc_closed;

/*
 * Prototypes
 */ 
void netconf_ok_set(int ok);
int netconf_ok_get(void);

int add_preamble(cbuf *xf);
int add_postamble(cbuf *xf);
int add_error_preamble(cbuf *xf, char *reason);
int detect_endtag(char *tag, char ch, int *state);
char *get_target(clicon_handle h, struct xml_node *xn, char *path);
int add_error_postamble(cbuf *xf);
int netconf_output(int s, cbuf *xf, char *msg);

#endif  /* _NETCONF_LIB_H_ */
