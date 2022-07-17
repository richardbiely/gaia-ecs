#!/bin/bash

PATH_BASE="build-clang"
mkdir ${PATH_BASE} -p

####################################################################
# Compiler
####################################################################

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

####################################################################
# Build the project
####################################################################

BUILD_SETTINGS_COMMON="-DGAIA_BUILD_UNITTEST=ON -DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF"
PATH_DEBUG="./${PATH_BASE}/debug"
PATH_RELEASE="./${PATH_BASE}/release"
PATH_RELEASE_ADDR="./${PATH_BASE}/release-addr"
PATH_RELEASE_MEM="./${PATH_BASE}/release-mem"

# Debug mode
cmake -E make_directory ${PATH_DEBUG}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_DEBUG}
cmake --build ${PATH_DEBUG} --config Debug

# Release mode
cmake -E make_directory ${PATH_RELEASE}
cmake -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_RELEASE}
cmake --build ${PATH_RELEASE} --config Release

# Release mode - adress sanitizers
cmake -E make_directory ${PATH_RELEASE_ADDR}
cmake -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON} -DGAIA_DEBUG=1 -DUSE_SANITIZER='Address;Undefined' -S .. -B ${PATH_RELEASE_ADDR}
cmake --build ${PATH_RELEASE_ADDR} --config Release

# Release mode - memory sanitizers
cmake -E make_directory ${PATH_RELEASE_MEM}
cmake -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON} -DGAIA_DEBUG=1 -DUSE_SANITIZER='Memory;MemoryWithOrigins' -S .. -B ${PATH_RELEASE_MEM}
cmake --build ${PATH_RELEASE_MEM} --config Release

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

# Release mode - address sanitizes
chmod +x ${PATH_RELEASE_ADDR}/${OUTPUT_BASE}
${PATH_RELEASE_ADDR}/${OUTPUT_BASE}

# Release mode - memory sanitizes
chmod +x ${PATH_RELEASE_MEM}/${OUTPUT_BASE}
${PATH_RELEASE_MEM}/${OUTPUT_BASE}