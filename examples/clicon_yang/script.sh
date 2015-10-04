#!/bin/bash
# Run this script after make but _before_ docker build
cp /usr/local/lib/libcligen.so.3.5 .
cp /usr/local/lib/libclicon.so.3.0 .
cp /usr/local/lib/libclicon_cli.so.3.0 .
cp /usr/local/bin/clicon_cli .
touch nullfile # dummy file to create docker dir
make clicon_yang.conf
