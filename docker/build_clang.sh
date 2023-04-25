#!/bin/bash

PATH_BASE="build-clang"

while getopts ":c" flag; do
    case "${flag}" in
        c) # remove the build direcetory
          rm -rf ${PATH_BASE};;
        \?) # invalid option
         echo "Error: Invalid option"
         exit;;
    esac
done

mkdir ${PATH_BASE} -p

####################################################################
# Compiler
####################################################################

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

####################################################################
# Build the project
####################################################################

# Build parameters
BUILD_SETTINGS_COMMON_BASE="-DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -DGAIA_PROFILER_BUILD=OFF"
BUILD_SETTINGS_COMMON="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=ON -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF"
BUILD_SETTINGS_COMMON_PROF="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_PROFILER_CPU=ON -DGAIA_PROFILER_MEM=ON"
# For sanitizer builds we have to turn off unit tests because Catch2 generates unitialized memory alerts
BUILD_SETTINGS_COMMON_SANI="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=OFF -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF -DGAIA_ECS_CHUNK_ALLOCATOR=0"

# Paths
PATH_DEBUG="./${PATH_BASE}/debug"
PATH_DEBUG_PROF="${PATH_DEBUG}-prof"
PATH_RELEASE="./${PATH_BASE}/release"
PATH_RELEASE_ADDR="${PATH_RELEASE}-addr"
PATH_RELEASE_MEM="${PATH_RELEASE}-mem"

# Debug mode
cmake -E make_directory ${PATH_DEBUG}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_DEBUG}
cmake --build ${PATH_DEBUG} --config Debug

# Debug mode + profiler
cmake -E make_directory ${PATH_DEBUG_PROF}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON_PROF} -S .. -B ${PATH_DEBUG_PROF}
cmake --build ${PATH_DEBUG_PROF} --config Debug

# Release mode
cmake -E make_directory ${PATH_RELEASE}
cmake -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_RELEASE}
cmake --build ${PATH_RELEASE} --config Release

# Release mode - adress sanitizers
cmake -E make_directory ${PATH_RELEASE_ADDR}
cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON_SANI} -DUSE_SANITIZER='Address;Undefined' -S .. -B ${PATH_RELEASE_ADDR}
cmake --build ${PATH_RELEASE_ADDR} --config Release

# Release mode - memory sanitizers
cmake -E make_directory ${PATH_RELEASE_MEM}
cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON_SANI} -DUSE_SANITIZER='Memory;MemoryWithOrigins' -S .. -B ${PATH_RELEASE_MEM}
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