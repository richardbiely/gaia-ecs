#!/bin/bash

mkdir "build-clang" -p

####################################################################
# Compiler
####################################################################

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

####################################################################
# Build the project
####################################################################

# Debug mode
cmake -E make_directory "./build-clang/debug"
cmake -DCMAKE_BUILD_TYPE=Debug -DGAIA_BUILD_UNITTEST=ON -DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -S .. -B "./build-clang/debug"
cmake --build "./build-clang/debug" --config Debug

# Release mode
cmake -E make_directory "./build-clang/release"
cmake -DCMAKE_BUILD_TYPE=Release -DGAIA_BUILD_UNITTEST=ON -DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -S .. -B "./build-clang/release"
cmake --build "./build-clang/release" --config Release

####################################################################
# Run unit tests
####################################################################

# Debug mode
chmod +x ./build-clang/debug/src/test/gaia_test
./build-clang/debug/src/test/gaia_test

# Release mode
chmod +x ./build-clang/release/src/test/gaia_test
./build-clang/release/src/test/gaia_test