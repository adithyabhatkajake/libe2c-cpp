#!/bin/bash

# Clean Project Root
rm -rf CMakeCache.txt CMakeFiles/ CMakeDoxyfile.in CMakeDoxygenDefaults.cmake cmake_install.cmake liblibe2c.a Makefile build/ libsecp256k1-prefix/
# Clean Test Directory
rm -rf test/{Makefile,cmake_install.cmake,test_secp256k1}
