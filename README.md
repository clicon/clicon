CLICON
======

CLICON is an automatic configuration manager including CLI
generation, transaction support, YANG, NETCONF and embedded databases.

Presentations and tutorial is found on the [CLICON project
page](http://www.clicon.org)

A typical installation is as follows:

    > configure	       	        # Configure clicon to platform
    > make                      # Compile
    > sudo make install         # Install libs, binaries, and config-files
    > sudo make install-include # Install include files (for compiling)

Several example applications are provided, including Hello, NTP, and datamodel.

CLIgen [CLIgen](http://www.cligen.se) is required for building CLICON. If you need 
to build and install CLIgen: 

    git clone https://github.com/olofhagsand/cligen.git
    cd cligen; configure; make; make install

CLICON is covered by GPLv3, and is also available with commercial license.

See COPYING for license, CHANGELOG for recent changes.




