CLICON
======

CLICON is a software suite for configuration management including CLI
generation, transaction support, netconf and embedded databases.

Presentations and tutorial is found on the [CLICON project
page](http://www.hagsand.se/clicon)

A typical installation is as follows:

    > configure	       	        # Configure clicon to platform
    > make		       	# Compile
    > sudo make install		# Install libs, binaries, and config-files
    > sudo make install-include # Install include files (for compiling)

Several example applications are provided, including Hello, NTP, and datamodel.

CLIgen is required for building CLICON. If you need to build
and install CLIgen: make cligen. 
To install CLIgen manually:

    git clone https://github.com/olofhagsand/cligen.git
    cd cligen; configure; make; make install

CLICON is covered by GPLv3, but can be obtained with a commercial license.

See COPYING for license, CHANGELOG for recent changes.




