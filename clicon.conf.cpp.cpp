# 
# CVS Version: $Id: clicon.conf.in,v 1.10 2013/09/19 15:02:05 olof Exp $ 
#
# Copyright (C) 2009-2013 Olof Hagsand and Benny Holmgren
#
# This file is part of CLICON.
#
# CLICON is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# CLICON is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with CLICON; see the file COPYING.  If not, see
# <http://www.gnu.org/licenses/>.
#
# CLICON options - Default values
# See clicon_tutorial for more documentation

# APPDIR is where config-file is found. Here for reference only.
# Default-value may be overriden by:
# configure-option --with-appdir or
# -a option to binaries, or
# Setting CLICON_APPDIR environment variable in the shell
# CLICON_APPDIR /usr/local/share/clicon

# Location of configuration-file for default values (this file)
CLICON_CONFIGFILE      sysconfdir/APPNAME.conf

# Database specification file syntax, Parse-tree or key. PT(default) or KEY(old)
# CLICON_DBSPEC_TYPE      PT

# Database specification file. Syntax either CLI or KEY given by CLICON_DBSPEC_TYPE
CLICON_DBSPEC_FILE     prefix/share/APPNAME/datamodel.spec

# Candidate qdbm database
CLICON_CANDIDATE_DB    localstatedir/APPNAME/candidate_db

# Running qdbm database
CLICON_RUNNING_DB      localstatedir/APPNAME/running_db

# Location of backend .so plugins
CLICON_BACKEND_DIR     libdir/APPNAME/backend

# Location of netconf (frontend) .so plugins
CLICON_NETCONF_DIR     libdir/APPNAME/netconf 

# Location of cli fronted .so plugins
CLICON_FRONTEND_DIR    libdir/APPNAME/frontend

# Location of frontend .cli cligen spec files
CLICON_CLI_DIR    libdir/APPNAME/frontend

# XXX Directory where to save configuration commit history (in XML). Snapshots
# are saved chronolically
CLICON_ARCHIVE_DIR      localstatedir/APPNAME/archive

# XXX Name of startup configuration file (in XML)
CLICON_STARTUP_CONFIG   localstatedir/APPNAME/startup-config

# XXX Unix socket for communicating with clicon_config
CLICON_SOCK         localstatedir/APPNAME/APPNAME.sock

# Process-id file
CLICON_BACKEND_PIDFILE  localstatedir/APPNAME/APPNAME.pidfile

# Group membership to access clicon_config unix socket
# CLICON_SOCK_GROUP       clicon

# Set if all configuration changes are committed directly, commit command unnecessary
# CLICON_AUTOCOMMIT       0

# Name of master plugin (both frontend and backend). Master plugin has special 
# callbacks for frontends. See clicon user manual for more info.
# CLICON_MASTER_PLUGIN    master

# Optional name of syntax group to start from if many. 
# Should be subdir of CLICON_CLI_DIR. Only frontend.
# CLICON_CLI_GROUP

# Startup CLI mode
# CLICON_CLI_MODE 

# Generate code for CLI completion of existing db symbols. Add name="myspec" in 
# datamodel spec and reference as @myspec.
# CLICON_CLI_GENMODEL     1

# Generate code for CLI completion of existing db symbols
# CLICON_CLI_GENMODEL_COMPLETION 0

# How to generate and show CLI syntax: VARS|ALL
# CLICON_CLI_GENMODEL_TYPE   VARS

# If 0, all model variables are mandatory, if 1 all are optional
# CLICON_CLI_GENMODEL_OPTIONAL 0

# Comment character in CLI
# CLICON_CLI_COMMENT      #

# Dont include keys in cvec in cli vars callbacks, ie a & k in 'a <b> k <c>' ignored
# CLICON_CLI_VARONLY      1


