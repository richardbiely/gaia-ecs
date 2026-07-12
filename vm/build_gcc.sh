#!/bin/bash

set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

PATH_BASE="build-gcc"
SANITIZER_ONLY=0

while getopts ":cs" flag; do
    case "${flag}" in
        c) # remove the build directory
          rm -rf ${PATH_BASE};;
        s) SANITIZER_ONLY=1;;
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

# Build parameters
BUILD_SETTINGS_COMMON_BASE="-DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=OFF -DGAIA_GENERATE_CC=OFF -DGAIA_MAKE_SINGLE_HEADER=OFF -DGAIA_PROFILER_BUILD=OFF -DGAIA_GENERATE_DOCS=OFF"
BUILD_SETTINGS_COMMON="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=ON -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF"

# Doctest triggers a few use-of-uninitialized-value alerts. However, we still want to run sanitizer so we will accept that for now.
# Maybe they are false alerts happening due to us not linking against a standard library built with memory sanitizers enabled. 
# TODO: Build custom libstdc++ with msan enabled?
BUILD_SETTINGS_COMMON_SANI="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_BUILD_UNITTEST=ON -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF -DGAIA_ECS_CHUNK_ALLOCATOR=OFF -DGAIA_DEVMODE=OFF"
BUILD_TARGET_UNIT=(--target gaia_test)
BUILD_TARGETS_SANI=(--target gaia_test gaia_perf gaia_duel gaia_app gaia_mt)

# Paths
PATH_DEBUG="./${PATH_BASE}/debug"
PATH_DEBUG20="./${PATH_BASE}/debug20"
PATH_DEBUG23="./${PATH_BASE}/debug23"
PATH_DEBUG_ADDR="${PATH_DEBUG}-addr"
PATH_RELEASE_ADDR="./${PATH_BASE}/release-addr"

# Sanitizer settings
SANI_ADDR="'Address;Undefined'"

if [[ ${SANITIZER_ONLY} -eq 0 ]]; then
    # Debug mode
    cmake -E make_directory ${PATH_DEBUG}
    cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DGAIA_USE_SANITIZER="" -DGAIA_DEVMODE=ON -S .. -B ${PATH_DEBUG}
    if ! cmake --build ${PATH_DEBUG} --config Debug "${BUILD_TARGET_UNIT[@]}"; then
        echo "${PATH_DEBUG} build failed"
        exit 1
    fi

    # Debug mode C++20
    cmake -E make_directory ${PATH_DEBUG20}
    cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DCMAKE_CXX_STANDARD=20 -DGAIA_USE_SANITIZER="" -DGAIA_DEVMODE=ON -S .. -B ${PATH_DEBUG20}
    if ! cmake --build ${PATH_DEBUG20} --config Debug "${BUILD_TARGET_UNIT[@]}"; then
        echo "${PATH_DEBUG20} build failed"
        exit 1
    fi

    # Debug mode C++23
    cmake -E make_directory ${PATH_DEBUG23}
    cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON} -DCMAKE_CXX_STANDARD=23 -DGAIA_USE_SANITIZER="" -DGAIA_DEVMODE=ON -S .. -B ${PATH_DEBUG23}
    if ! cmake --build ${PATH_DEBUG23} --config Debug "${BUILD_TARGET_UNIT[@]}"; then
        echo "${PATH_DEBUG23} build failed"
        exit 1
    fi
fi

# Debug mode - address sanitizers
cmake -E make_directory ${PATH_DEBUG_ADDR}
cmake -DCMAKE_BUILD_TYPE=Debug ${BUILD_SETTINGS_COMMON_SANI} -DGAIA_USE_SANITIZER=${SANI_ADDR} -S .. -B ${PATH_DEBUG_ADDR}
if ! cmake --build ${PATH_DEBUG_ADDR} --config Debug "${BUILD_TARGETS_SANI[@]}"; then
    echo "${PATH_DEBUG_ADDR} build failed"
    exit 1
fi

# Release mode - address sanitizers
cmake -E make_directory ${PATH_RELEASE_ADDR}
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ${BUILD_SETTINGS_COMMON_SANI} -DGAIA_USE_SANITIZER=${SANI_ADDR} -S .. -B ${PATH_RELEASE_ADDR}
if ! cmake --build ${PATH_RELEASE_ADDR} --config RelWithDebInfo "${BUILD_TARGETS_SANI[@]}"; then
    echo "${PATH_RELEASE_ADDR} build failed"
    exit 1
fi

####################################################################
# Run analysis
####################################################################

# Print a detailed report and fail the managed profile on the first sanitizer hit.
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:report_error_type=1
export ASAN_OPTIONS=halt_on_error=1:detect_stack_use_after_return=1:detect_container_overflow=1:check_initialization_order=1:strict_init_order=1
export ASAN_SYMBOLIZER_PATH=$(which addr2line)

UNIT_TEST_PATH="src/test/gaia_test"
PERF_PATH="src/perf/perf/gaia_perf"
PERF_DUEL_PATH="src/perf/duel/gaia_duel"
PERF_APP_PATH="src/perf/app/gaia_app"
PERF_MT_PATH="src/perf/mt/gaia_mt"

SANITIZER_TASKS=()

queue_sanitized_binary() {
    local task_id="$1"
    local label="$2"
    local binary="$3"
    SANITIZER_TASKS+=(
        --task "${task_id}-debug-addr" "${label} - Debug Address/Undefined" "${PATH_DEBUG_ADDR}/${binary}" -s
        --task "${task_id}-release-addr" "${label} - RelWithDebInfo Address/Undefined" "${PATH_RELEASE_ADDR}/${binary}" -s
    )
}

if [[ ${SANITIZER_ONLY} -eq 0 ]]; then
    echo ${PATH_DEBUG}/${UNIT_TEST_PATH}
    echo "Debug mode"
    chmod +x ${PATH_DEBUG}/${UNIT_TEST_PATH}
    ${PATH_DEBUG}/${UNIT_TEST_PATH}

    echo ${PATH_DEBUG20}/${UNIT_TEST_PATH}
    echo "Debug mode C++20"
    chmod +x ${PATH_DEBUG20}/${UNIT_TEST_PATH}
    ${PATH_DEBUG20}/${UNIT_TEST_PATH}

    echo ${PATH_DEBUG23}/${UNIT_TEST_PATH}
    echo "Debug mode C++23"
    chmod +x ${PATH_DEBUG23}/${UNIT_TEST_PATH}
    ${PATH_DEBUG23}/${UNIT_TEST_PATH}
fi

queue_sanitized_binary "unit" "Unit tests" "${UNIT_TEST_PATH}"
queue_sanitized_binary "perf" "Performance tests" "${PERF_PATH}"
queue_sanitized_binary "duel" "Duel benchmark" "${PERF_DUEL_PATH}"
queue_sanitized_binary "app" "Application benchmark" "${PERF_APP_PATH}"
queue_sanitized_binary "mt" "Multithreaded benchmark" "${PERF_MT_PATH}"

python3 "${SCRIPT_DIR}/run_tasks.py" --jobs "${GAIA_SANITIZER_JOBS:-2}" "${SANITIZER_TASKS[@]}"
