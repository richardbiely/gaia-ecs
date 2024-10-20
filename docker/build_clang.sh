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

export CC="/usr/bin/clang"
export CXX="/usr/bin/clang++"

####################################################################
# Build the project
####################################################################

# Build parameters
BUILD_SETTINGS_COMMON_BASE="-DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=ON -DGAIA_GENERATE_CC=OFF -DGAIA_MAKE_SINGLE_HEADER=OFF -DGAIA_PROFILER_BUILD=OFF"
BUILD_SETTINGS_COMMON="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=ON -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF"
BUILD_SETTINGS_COMMON_PROF="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=OFF -DGAIA_PROFILER_CPU=ON -DGAIA_PROFILER_MEM=ON"
# For sanitizer builds we have to turn off unit tests because Catch2 generates unitialized memory alerts.
# These are false alerts happening due to us not linking against a standard library built with memory sanitizers enabled. 
# TODO: Build custom libc++ with msan enabled
BUILD_SETTINGS_COMMON_SANI="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=OFF -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF -DGAIA_ECS_CHUNK_ALLOCATOR=OFF"

# Paths
PATH_DEBUG="./${PATH_BASE}/debug"
PATH_DEBUG_SYSA="./${PATH_BASE}/debug-sysa"
PATH_DEBUG_PROF="${PATH_DEBUG}-prof"
PATH_RELEASE="./${PATH_BASE}/release"
PATH_DEBUG_ADDR="${PATH_DEBUG}-addr"
PATH_DEBUG_MEM="${PATH_DEBUG}-mem"
PATH_RELEASE_ADDR="${PATH_RELEASE}-addr"
PATH_RELEASE_MEM="${PATH_RELEASE}-mem"

# Sanitizer settings
SANI_ADDR="'Address;Undefined'"
SANI_MEM="'MemoryWithOrigins'"

# Debug mode
cmake -E make_directory ${PATH_DEBUG}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DGAIA_DEVMODE=ON -S .. -B ${PATH_DEBUG}
if ! cmake --build ${PATH_DEBUG} --config Debug; then
    echo "${PATH_DEBUG} build failed"
    exit 1
fi

# Debug mode + system allocator
cmake -E make_directory ${PATH_DEBUG_SYSA}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DGAIA_DEVMODE=ON -DGAIA_ECS_CHUNK_ALLOCATOR=OFF -S .. -B ${PATH_DEBUG_SYSA}
if ! cmake --build ${PATH_DEBUG_SYSA} --config Debug; then
    echo "${PATH_DEBUG_SYSA} build failed"
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

# Debug mode - address sanitizers
cmake -E make_directory ${PATH_DEBUG_ADDR}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON_SANI} -DGAIA_USE_SANITIZER=${SANI_ADDR} -S .. -B ${PATH_DEBUG_ADDR}
if ! cmake --build ${PATH_DEBUG_ADDR} --config Debug; then
    echo "${PATH_DEBUG_ADDR} build failed"
    exit 1
fi

# Debug mode - memory sanitizers
cmake -E make_directory ${PATH_DEBUG_MEM}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON_SANI} -DGAIA_USE_SANITIZER=${SANI_MEM} -S .. -B ${PATH_DEBUG_MEM}
if ! cmake --build ${PATH_DEBUG_MEM} --config Debug; then
    echo "${PATH_DEBUG_MEM} build failed"
    exit 1
fi

# Release mode - address sanitizers
cmake -E make_directory ${PATH_RELEASE_ADDR}
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ${BUILD_SETTINGS_COMMON_SANI} -DGAIA_USE_SANITIZER=${SANI_ADDR} -S .. -B ${PATH_RELEASE_ADDR}
if ! cmake --build ${PATH_RELEASE_ADDR} --config RelWithDebInfo; then
    echo "${PATH_RELEASE_ADDR} build failed"
    exit 1
fi

# Release mode - memory sanitizers
cmake -E make_directory ${PATH_RELEASE_MEM}
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ${BUILD_SETTINGS_COMMON_SANI} -DGAIA_USE_SANITIZER=${SANI_MEM} -S .. -B ${PATH_RELEASE_MEM}
if ! cmake --build ${PATH_RELEASE_MEM} --config RelWithDebInfo; then
    echo "${PATH_RELEASE_MEM} build failed"
    exit 1
fi

####################################################################
# Run analysis
####################################################################

PERF_ENTITY_PATH="src/perf/entity/gaia_perf_entity"
PERF_ITER_PATH="src/perf/iter/gaia_perf_iter"
PERF_DUEL_PATH="src/perf/duel/gaia_perf_duel"
PERF_APP_PATH="src/perf/app/gaia_perf_app"

echo ${PATH_DEBUG_ADDR}/${PERF_ENTITY_PATH}
echo "Debug mode + addr sanitizer"
chmod +x ${PATH_DEBUG_ADDR}/${PERF_ENTITY_PATH}
${PATH_DEBUG_ADDR}/${PERF_ENTITY_PATH} -s
echo "Debug mode + mem sanitizer"
chmod +x ${PATH_DEBUG_MEM}/${PERF_ENTITY_PATH}
${PATH_DEBUG_MEM}/${PERF_ENTITY_PATH} -s
echo "Release mode + addr sanitizer"
chmod +x ${PATH_RELEASE_ADDR}/${PERF_ENTITY_PATH}
${PATH_RELEASE_ADDR}/${PERF_ENTITY_PATH} -s
echo "Release mode + mem sanitizer"
chmod +x ${PATH_RELEASE_MEM}/${PERF_ENTITY_PATH}
${PATH_RELEASE_MEM}/${PERF_ENTITY_PATH} -s

echo ${PATH_DEBUG_ADDR}/${PERF_ITER_PATH}
echo "Debug mode + addr sanitizer"
chmod +x ${PATH_DEBUG_ADDR}/${PERF_ITER_PATH}
${PATH_DEBUG_ADDR}/${PERF_ITER_PATH} -s
echo "Debug mode + mem sanitizer"
chmod +x ${PATH_DEBUG_MEM}/${PERF_ITER_PATH}
${PATH_DEBUG_MEM}/${PERF_ITER_PATH} -s
echo "Release mode + addr sanitizer"
chmod +x ${PATH_RELEASE_ADDR}/${PERF_ITER_PATH}
${PATH_RELEASE_ADDR}/${PERF_ITER_PATH} -s
echo "Release mode + mem sanitizer"
chmod +x ${PATH_RELEASE_MEM}/${PERF_ITER_PATH}
${PATH_RELEASE_MEM}/${PERF_ITER_PATH} -s

echo ${PATH_DEBUG_ADDR}/${PERF_DUEL_PATH}
echo "Debug mode + addr sanitizer"
chmod +x ${PATH_DEBUG_ADDR}/${PERF_DUEL_PATH}
${PATH_DEBUG_ADDR}/${PERF_DUEL_PATH} -s
echo "Debug mode + mem sanitizer"
chmod +x ${PATH_DEBUG_MEM}/${PERF_DUEL_PATH}
${PATH_DEBUG_MEM}/${PERF_DUEL_PATH} -s
echo "Release mode + addr sanitizer"
chmod +x ${PATH_RELEASE_ADDR}/${PERF_DUEL_PATH}
${PATH_RELEASE_ADDR}/${PERF_DUEL_PATH} -s
echo "Release mode + mem sanitizer"
chmod +x ${PATH_RELEASE_MEM}/${PERF_DUEL_PATH}
${PATH_RELEASE_MEM}/${PERF_DUEL_PATH} -s

echo ${PATH_DEBUG_ADDR}/${PERF_APP_PATH}
echo "Debug mode + addr sanitizer"
chmod +x ${PATH_DEBUG_ADDR}/${PERF_APP_PATH}
${PATH_DEBUG_ADDR}/${PERF_APP_PATH} -s
echo "Debug mode + mem sanitizer"
chmod +x ${PATH_DEBUG_MEM}/${PERF_APP_PATH}
${PATH_DEBUG_MEM}/${PERF_APP_PATH} -s
echo "Release mode + addr sanitizer"
chmod +x ${PATH_RELEASE_ADDR}/${PERF_APP_PATH}
${PATH_RELEASE_ADDR}/${PERF_APP_PATH} -s
echo "Release mode + mem sanitizer"
chmod +x ${PATH_RELEASE_MEM}/${PERF_APP_PATH}
${PATH_RELEASE_MEM}/${PERF_APP_PATH} -s