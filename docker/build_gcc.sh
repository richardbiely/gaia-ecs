#!/bin/bash

mkdir "build-gcc" -p

####################################################################
# Compiler
####################################################################

export CC=/usr/bin/gcc-7
export CXX=/usr/bin/g++-7

####################################################################
# Build the project
####################################################################

# Debug mode
cmake -E make_directory "./build-gcc/debug"
cmake -DCMAKE_BUILD_TYPE=Debug -DGAIA_BUILD_UNITTEST=ON -DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -S .. -B "./build-gcc/debug"
cmake --build "./build-gcc/debug" --config Debug

# Release mode
cmake -E make_directory "./build-gcc/release"
cmake -DCMAKE_BUILD_TYPE=Release -DGAIA_BUILD_UNITTEST=ON -DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -S .. -B "./build-gcc/release"
cmake --build "./build-gcc/release" --config Release

####################################################################
# Run unit tests
####################################################################

# Debug mode
chmod +x ./build-gcc/debug/src/test/gaia_test
./build-gcc/debug/src/test/gaia_test

# Release mode
chmod +x ./build-gcc/release/src/test/gaia_test
./build-gcc/release/src/test/gaia_test