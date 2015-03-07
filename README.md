CLICON
======

CLICON is an automatic configuration manager where you from a YANG
specification generate interactive CLI, NETCONF and embedded
databases with transaction support.

Presentations and tutorial is found on the [CLICON project
page](http://www.clicon.org)

A typical installation is as follows:

    > configure	       	        # Configure clicon to platform
    > make                      # Compile
    > sudo make install         # Install libs, binaries, and config-files
    > sudo make install-include # Install include files (for compiling)

Several example applications are provided, including Hello, NTP,
datamodel and yang router. See also [ROST](https://github.com/clicon/rost) which is an
open-source router using CLICON. It all origins from work at
[KTH](http://www.csc.kth.se/~olofh/10G_OSR)

[CLIgen](http://www.cligen.se) is required for building CLICON. If you need 
to build and install CLIgen: 

    git clone https://github.com/olofhagsand/cligen.git
    cd cligen; configure; make; make install

CLICON is covered by GPLv3, and is also available with commercial license.

See COPYING for license, CHANGELOG for recent changes.




