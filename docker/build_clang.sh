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
PATH_DEBUG_PROF="${PATH_DEBUG}_prof"
PATH_RELEASE="./${PATH_BASE}/release"
PATH_RELEASE_ADDR="./${PATH_BASE}/release-addr"
PATH_RELEASE_MEM="./${PATH_BASE}/release-mem"

# Debug mode
cmake -E make_directory ${PATH_DEBUG}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_DEBUG}
cmake --build ${PATH_DEBUG} --config Debug

# Debug mode + profiler
cmake -E make_directory ${PATH_DEBUG_PROF}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DGAIA_PROFILER_CPU=ON -DGAIA_PROFILER_MEM=ON -S .. -B ${PATH_DEBUG_PROF}
cmake --build ${PATH_DEBUG_PROF} --config Debug

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

UNIT_TEST_PATH="src/test/gaia_test"

# Debug mode
chmod +x ${PATH_DEBUG}/${UNIT_TEST_PATH}
${PATH_DEBUG}/${UNIT_TEST_PATH}

# Debug mode + profiler
chmod +x ${PATH_DEBUG_PROF}/${UNIT_TEST_PATH}
${PATH_DEBUG_PROF}/${UNIT_TEST_PATH}

# Release mode
chmod +x ${PATH_RELEASE}/${UNIT_TEST_PATH}
${PATH_RELEASE}/${UNIT_TEST_PATH}

# Release mode - address sanitizes
chmod +x ${PATH_RELEASE_ADDR}/${UNIT_TEST_PATH}
${PATH_RELEASE_ADDR}/${UNIT_TEST_PATH}

# Release mode - memory sanitizes
chmod +x ${PATH_RELEASE_MEM}/${UNIT_TEST_PATH}
${PATH_RELEASE_MEM}/${UNIT_TEST_PATH}