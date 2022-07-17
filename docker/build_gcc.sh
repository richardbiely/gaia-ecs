#!/bin/bash

PATH_BASE="build-gcc"
mkdir ${PATH_BASE} -p

####################################################################
# Compiler
####################################################################

export CC=/usr/bin/gcc-7
export CXX=/usr/bin/g++-7

####################################################################
# Build the project
####################################################################

BUILD_SETTINGS_COMMON="-DGAIA_BUILD_UNITTEST=ON -DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF"
PATH_DEBUG="./${PATH_BASE}/debug"
PATH_RELEASE="./${PATH_BASE}/release"

# Debug mode
cmake -E make_directory ${PATH_DEBUG}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_DEBUG}
cmake --build ${PATH_DEBUG} --config Debug

# Release mode
cmake -E make_directory ${PATH_RELEASE}
cmake -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_RELEASE}
cmake --build ${PATH_RELEASE} --config Release

####################################################################
# Run unit tests
####################################################################

OUTPUT_BASE="src/test/gaia_test"

# Debug mode
chmod +x ${PATH_DEBUG}/${OUTPUT_BASE}
${PATH_DEBUG}/${OUTPUT_BASE}

# Release mode
chmod +x ${PATH_RELEASE}/${OUTPUT_BASE}
${PATH_RELEASE}/${OUTPUT_BASE}