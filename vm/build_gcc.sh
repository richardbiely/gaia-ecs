#!/bin/bash

PATH_BASE="build-gcc"

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

export CC="/usr/bin/gcc"
export CXX="/usr/bin/g++"
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)

####################################################################
# Build the project
####################################################################

BUILD_SETTINGS_COMMON_BASE="-DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -DGAIA_MAKE_SINGLE_HEADER=OFF -DGAIA_PROFILER_BUILD=OFF"
BUILD_SETTINGS_COMMON="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=ON -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF"
BUILD_SETTINGS_COMMON_PROF="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=OFF -DGAIA_PROFILER_CPU=ON -DGAIA_PROFILER_MEM=ON"
PATH_DEBUG="./${PATH_BASE}/debug"
PATH_DEBUG20="./${PATH_BASE}/debug20"
PATH_DEBUG23="./${PATH_BASE}/debug23"
PATH_DEBUG_PROF="${PATH_DEBUG}-prof"
PATH_RELEASE="./${PATH_BASE}/release"

# Debug mode
cmake -E make_directory ${PATH_DEBUG}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DGAIA_DEVMODE=ON -S .. -B ${PATH_DEBUG}
if ! cmake --build ${PATH_DEBUG} --config Debug; then
    echo "${PATH_DEBUG} build failed"
    exit 1
fi

# Debug mode C++20
cmake -E make_directory ${PATH_DEBUG20}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DCMAKE_CXX_STANDARD=20 -DGAIA_DEVMODE=ON -S .. -B ${PATH_DEBUG20}
if ! cmake --build ${PATH_DEBUG20} --config Debug; then
    echo "${PATH_DEBUG20} build failed"
    exit 1
fi

# Debug mode C++23
cmake -E make_directory ${PATH_DEBUG23}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DCMAKE_CXX_STANDARD=23 -DGAIA_DEVMODE=ON -S .. -B ${PATH_DEBUG23}
if ! cmake --build ${PATH_DEBUG23} --config Debug; then
    echo "${PATH_DEBUG23} build failed"
    exit 1
fi

# Debug mode + profiler
cmake -E make_directory ${PATH_DEBUG_PROF}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON_PROF} -DGAIA_DEVMODE=ON -S .. -B ${PATH_DEBUG_PROF}
if ! cmake --build ${PATH_DEBUG_PROF} --config Debug; then
    echo "${PATH_DEBUG_PROF} build failed"
    exit 1
fi

# Release mode
cmake -E make_directory ${PATH_RELEASE}
cmake -DCMAKE_BUILD_TYPE=Release ${BUILD_SETTINGS_COMMON} -S .. -B ${PATH_RELEASE}
if ! cmake --build ${PATH_RELEASE} --config Release; then
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

# Debug mode C++20
chmod +x ${PATH_DEBUG20}/${UNIT_TEST_PATH}
${PATH_DEBUG20}/${UNIT_TEST_PATH}

# Debug mode C++23
chmod +x ${PATH_DEBUG23}/${UNIT_TEST_PATH}
${PATH_DEBUG23}/${UNIT_TEST_PATH}

# Debug mode
chmod +x ${PATH_DEBUG}/${UNIT_TEST_PATH}
${PATH_DEBUG}/${UNIT_TEST_PATH}

# Debug mode
chmod +x ${PATH_DEBUG}/${UNIT_TEST_PATH}
${PATH_DEBUG}/${UNIT_TEST_PATH}

# Release mode
chmod +x ${PATH_RELEASE}/${UNIT_TEST_PATH}
${PATH_RELEASE}/${UNIT_TEST_PATH}