#!/bin/bash

# Clean Project Root
rm -rf CMakeCache.txt CMakeFiles/ CMakeDoxyfile.in CMakeDoxygenDefaults.cmake \
    cmake_install.cmake liblibe2c.a Makefile build/ libsecp256k1-prefix/
# Clean Test Directory
rm -rf test/{Makefile,cmake_install.cmake,test_secp256k1}
# Clean Programs Directory
rm -rf programs/{CMakeFiles,Makefile,cmake_install.cmake,e2c-app,e2c-client}
# Clean Binaries in the root directory
rm -rf tls-keygen keygen
