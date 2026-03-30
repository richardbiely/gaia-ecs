#!/usr/bin/env bash

set -euo pipefail

BUILD_TYPE="${1:-Debug}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_DIR="$REPO_ROOT/build/$BUILD_TYPE/src/test"

run_test() {
	local test_bin="$1"
	shift
	echo "==> $test_bin"
	"$TEST_DIR/$test_bin" "$@"
}

run_test "gaia_test"
run_test "gaia_test_no_autoreg" "--test-case=*component registration*"
