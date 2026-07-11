#!/bin/bash

set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

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
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)

####################################################################
# Build the project
####################################################################

BUILD_SETTINGS_COMMON_BASE="-DGAIA_BUILD_UNITTEST=OFF -DGAIA_BUILD_BENCHMARK=ON -DGAIA_BUILD_EXAMPLES=OFF -DGAIA_GENERATE_CC=OFF -DGAIA_MAKE_SINGLE_HEADER=OFF -DGAIA_PROFILER_BUILD=OFF -DGAIA_GENERATE_DOCS=OFF"
BUILD_SETTINGS_COMMON="${BUILD_SETTINGS_COMMON_BASE} -DGAIA_PROFILER_CPU=OFF -DGAIA_PROFILER_MEM=OFF"
PATH_RELEASE="./${PATH_BASE}/release-cachegrind"

# Release mode
cmake -E make_directory ${PATH_RELEASE}
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ${BUILD_SETTINGS_COMMON} -DGAIA_DEBUG=0 -S .. -B ${PATH_RELEASE}
if ! cmake --build ${PATH_RELEASE} --config RelWithDebInfo; then
    echo "${PATH_RELEASE} build failed"
    exit 1
fi

####################################################################
# Run cachegrind
####################################################################

OUTPUT_BASE="src/perf/duel/gaia_duel"
OUTPUT_ARGS_DOD="-p -dod"
OUTPUT_ARGS_ECS="-p"

VALGRIND_ARGS="--tool=cachegrind"

# Debug mode
chmod +x ${PATH_RELEASE}/${OUTPUT_BASE}

# We need to adjust how cachegrind is called based on what CPU we have.
CURRENT_PLATFORM=$(uname -p)
if [[ "$CURRENT_PLATFORM" =~ ^(i386|i686|x86_64)$ ]]; then
    # Most Intel and AMD CPUs should work just fine using a generic cachegrind call
    VALGRIND_ARGS_CUSTOM=""
else
    # If we are not an x86 CPU we assume ARM. Namely Apple M1.
    # Docker at least up to version 4.17 does not reliably propagate /proc/cpuinfo to the virtual machine.
    # M1 cache sizes therefore need to be supplied explicitly.
    # M1 efficiency core: I1=128kiB 8-way, D1=64kiB 8-way, L2=4MiB 16-way.
    VALGRIND_ARGS_CUSTOM="--I1=131072,8,128 --D1=65536,8,128 --L2=4194304,16,128 --cache-sim=yes"
fi

echo "Cachegrind - measuring DOD performance"
valgrind ${VALGRIND_ARGS} ${VALGRIND_ARGS_CUSTOM} --cachegrind-out-file=cachegrind.out.dod --branch-sim=yes "${PATH_RELEASE}/${OUTPUT_BASE}" ${OUTPUT_ARGS_DOD}
cg_annotate --show=Dr,D1mr,DLmr --sort=Dr,D1mr,DLmr cachegrind.out.dod > cachegrind.r.dod # cache reads
cg_annotate --show=Dw,D1mw,DLmw --sort=Dw,D1mw,DLmw cachegrind.out.dod > cachegrind.w.dod # cache writes
cg_annotate --show=Bc,Bcm,Bi,Bim --sort=Bc,Bcm,Bi,Bim cachegrind.out.dod > cachegrind.b.dod # branch hits

echo "Cachegrind - measuring ECS performance"
valgrind ${VALGRIND_ARGS} ${VALGRIND_ARGS_CUSTOM} --cachegrind-out-file=cachegrind.out.ecs --branch-sim=yes "${PATH_RELEASE}/${OUTPUT_BASE}" ${OUTPUT_ARGS_ECS}
cg_annotate --show=Dr,D1mr,DLmr --sort=Dr,D1mr,DLmr cachegrind.out.ecs > cachegrind.r.ecs
cg_annotate --show=Dw,D1mw,DLmw --sort=Dw,D1mw,DLmw cachegrind.out.ecs > cachegrind.w.ecs
cg_annotate --show=Bc,Bcm,Bi,Bim --sort=Bc,Bcm,Bi,Bim cachegrind.out.ecs > cachegrind.b.ecs
