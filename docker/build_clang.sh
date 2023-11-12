#!/bin/bash

PATH_BASE="build-clang"

while getopts ":c" flag; do
    case "${flag}" in
        c) # remove the build directory
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

COMPILER_CXX="/usr/bin/clang++"
COMPILER_SETTINGS="-DCMAKE_CXX_COMPILER=${COMPILER_CXX}"

####################################################################
# Build the project
####################################################################

# Build parameters
BUILD_SETTINGS_COMMON_BASE="-DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -DGAIA_MAKE_SINGLE_HEADER=OFF -DGAIA_PROFILER_BUILD=OFF"
BUILD_SETTINGS_COMMON="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=ON -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF"
BUILD_SETTINGS_COMMON_PROF="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=OFF -DGAIA_PROFILER_CPU=ON -DGAIA_PROFILER_MEM=ON"
# For sanitizer builds we have to turn off unit tests because Catch2 generates unitialized memory alerts.
# These are false alerts happening due to us not linking against the standard library that is not build with
# memory sanitizers. 
# TODO: Build custom libc++ with msan enabled
BUILD_SETTINGS_COMMON_SANI="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=OFF -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF -DGAIA_ECS_CHUNK_ALLOCATOR=OFF"

# Paths
PATH_DEBUG="./${PATH_BASE}/debug"
PATH_DEBUG_SYSA="./${PATH_BASE}/debug-sysa"
PATH_DEBUG_PROF="${PATH_DEBUG}-prof"
PATH_RELEASE="./${PATH_BASE}/release"
PATH_RELEASE_ADDR="${PATH_RELEASE}-addr"
PATH_RELEASE_MEM="${PATH_RELEASE}-mem"

# Debug mode
cmake -E make_directory ${PATH_DEBUG}
cmake "${COMPILER_SETTINGS}" -DCMAKE_BUILD_TYPE=Debug "${BUILD_SETTINGS_COMMON}" -S .. -B ${PATH_DEBUG}
if ! cmake --build ${PATH_DEBUG} --config Debug; then
    echo "${PATH_DEBUG} build failed"
    exit 1
fi

# Debug mode + system allocator
cmake -E make_directory ${PATH_DEBUG_SYSA}
cmake "${COMPILER_SETTINGS}" -DCMAKE_BUILD_TYPE=Debug "${BUILD_SETTINGS_COMMON}" -DGAIA_ECS_CHUNK_ALLOCATOR=OFF -S .. -B ${PATH_DEBUG_SYSA}
if ! cmake --build ${PATH_DEBUG_SYSA} --config Debug; then
    echo "${PATH_DEBUG_SYSA} build failed"
    exit 1
fi

# Debug mode + profiler
cmake -E make_directory ${PATH_DEBUG_PROF}
cmake "${COMPILER_SETTINGS}" -DCMAKE_BUILD_TYPE=Debug "${BUILD_SETTINGS_COMMON_PROF}" -S .. -B ${PATH_DEBUG_PROF}
if ! cmake --build ${PATH_DEBUG_PROF} --config Debug; then
    echo "${PATH_DEBUG_PROF} build failed"
    exit 1
fi

# Release mode
cmake -E make_directory ${PATH_RELEASE}
cmake "${COMPILER_SETTINGS}" -DCMAKE_BUILD_TYPE=Release "${BUILD_SETTINGS_COMMON}" -S .. -B ${PATH_RELEASE}
if ! cmake --build ${PATH_RELEASE} --config Release; then
    echo "${PATH_RELEASE} build failed"
    exit 1
fi

# Release mode - adress sanitizers
cmake -E make_directory ${PATH_RELEASE_ADDR}
cmake "${COMPILER_SETTINGS}" -DCMAKE_BUILD_TYPE=RelWithDebInfo "${BUILD_SETTINGS_COMMON_SANI}" -DUSE_SANITIZER='Address;Undefined' -S .. -B ${PATH_RELEASE_ADDR}
if ! cmake --build ${PATH_RELEASE_ADDR} --config RelWithDebInfo; then
    echo "${PATH_RELEASE_ADDR} build failed"
    exit 1
fi

# Release mode - memory sanitizers
cmake -E make_directory ${PATH_RELEASE_MEM}
cmake "${COMPILER_SETTINGS}" -DCMAKE_BUILD_TYPE=RelWithDebInfo "${BUILD_SETTINGS_COMMON_SANI}" -DUSE_SANITIZER='Memory;MemoryWithOrigins' -S .. -B ${PATH_RELEASE_MEM}
if ! cmake --build ${PATH_RELEASE_MEM} --config RelWithDebInfo; then
    echo "${PATH_RELEASE_MEM} build failed"
    exit 1
fi

####################################################################
# Run unit tests
####################################################################

UNIT_TEST_PATH="src/test/gaia_test"

echo "Debug mode"
chmod +x ${PATH_DEBUG}/${UNIT_TEST_PATH}
${PATH_DEBUG}/${UNIT_TEST_PATH}

echo "Debug mode + system allocator"
chmod +x ${PATH_DEBUG_SYSA}/${UNIT_TEST_PATH}
${PATH_DEBUG_SYSA}/${UNIT_TEST_PATH}

echo "Release mode"
chmod +x ${PATH_RELEASE}/${UNIT_TEST_PATH}
${PATH_RELEASE}/${UNIT_TEST_PATH}
