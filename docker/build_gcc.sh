#!/bin/bash

PATH_BASE="build-gcc"

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

COMPILER_CC=/usr/bin/gcc-7
COMPILER_CXX=/usr/bin/g++-7
COMPILER_SETTINGS="-DCMAKE_CC_COMPILER=${COMPILER_CC} -DCMAKE_CXX_COMPILER=${COMPILER_CXX} --no-warn-unused-cli"

####################################################################
# Build the project
####################################################################

BUILD_SETTINGS_COMMON_BASE="-DGAIA_BUILD_UNITTEST=ON -DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -DGAIA_MAKE_SINGLE_HEADER=OFF -DGAIA_PROFILER_BUILD=OFF"
BUILD_SETTINGS_COMMON="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF"
BUILD_SETTINGS_COMMON_PROF="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_PROFILER_CPU=ON -DGAIA_PROFILER_MEM=ON"
PATH_DEBUG="./${PATH_BASE}/debug"
PATH_DEBUG_PROF="${PATH_DEBUG}-prof"
PATH_RELEASE="./${PATH_BASE}/release"

# Debug mode
cmake -E make_directory ${PATH_DEBUG}
cmake ${COMPILER_SETTINGS} -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_DEBUG}
cmake --build ${PATH_DEBUG} --config Debug
if [ $? -ne 0 ]; then
    echo "${PATH_DEBUG} build failed"
    exit 1
fi

# Debug mode + profiler
cmake -E make_directory ${PATH_DEBUG_PROF}
cmake ${COMPILER_SETTINGS} -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON_PROF} -S .. -B ${PATH_DEBUG_PROF}
cmake --build ${PATH_DEBUG_PROF} --config Debug
if [ $? -ne 0 ]; then
    echo "${PATH_DEBUG_PROF} build failed"
    exit 1
fi

# Release mode
cmake -E make_directory ${PATH_RELEASE}
cmake ${COMPILER_SETTINGS} -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_RELEASE}
cmake --build ${PATH_RELEASE} --config Release
if [ $? -ne 0 ]; then
    echo "${PATH_RELEASE} build failed"
    exit 1
fi

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