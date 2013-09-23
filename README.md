clicon
======

CLICON is a software suite for configuration management including CLI generation, netconf interface and embedded databases
$Id: README,v 1.17 2013/08/15 11:56:37 olof Exp $

This is CLICON source code repository

A typical installation is as follows:
> configure	       	        # Configure clicon to platform
> make		       	 	# Compile
> sudo make install		# Install libs, binaries, and config-files
> sudo make install-include 	# Install include files (for compiling)

CLIgen is required for building CLICON. If you need to build
and install CLIgen: make cligen. 
To install CLIgen manually:
# git clone https://github.com/olofhagsand/cligen.git
# cd cligen; configure; make; make install

See doc/tutorial/clicon_tutorial.pdf for documentation on how to use clicon.

See COPYING for license, CHANGELOG for recent changes.

You can contact the authors at olof@hagsand.se and bennyholmgren@gmail.com.


