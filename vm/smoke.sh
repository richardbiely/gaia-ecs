#!/bin/bash

set -euo pipefail

test -f CMakeLists.txt
cmake --version | head -n 1
clang --version | head -n 1
gcc --version | head -n 1
git --version
echo "Gaia-ECS container toolchain is ready"