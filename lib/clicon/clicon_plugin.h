/*
 * CVS Version: $Id: clicon_plugin.h,v 1.10 2013/08/31 06:38:22 benny Exp $
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

 */


#ifndef _CLICON_PLUGIN_H_
#define _CLICON_PLUGIN_H_

/*
 * Types
 */
/* The dynamicically loadable plugin object handle */
typedef void *plghndl_t;

/* Common plugin function declarations */
typedef int (plginit_t)(clicon_handle);	 	        /* Init */
typedef int (plgstart_t)(clicon_handle, int, char **);/* Plugin start */
typedef int (plgexit_t)(clicon_handle);		/* Plugin exit */

/* Find plugin by name callback.  XXX Should be clicon internal */
typedef void *(find_plugin_t)(clicon_handle, char *); 

/*
 * Prototypes
 */
/* Common plugin prototypes. Specific are in clicon_config_plugin.h and 
   clicon_cli_plugin.h, for example */
/* Called when plugin loaded. Only mandadory callback. All others optional */
int plugin_init(clicon_handle h);

/* Called just before plugin unloaded. */
int plugin_exit(clicon_handle h);

/* Called when backend started with cmd-line arguments from daemon call. */
int plugin_start(clicon_handle h, int argc, char **argv);

/* Find a function in global namespace or a plugin. XXX clicon internal */
void *clicon_find_func(clicon_handle h, char *plugin, char *func);

#endif  /* _CLICON_PLUGIN_H_ */
