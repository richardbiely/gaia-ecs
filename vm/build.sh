#!/bin/bash

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

if ! bash "$SCRIPT_DIR/build_clang.sh" "${@:1}"; then
  exit 1
fi

if ! bash "$SCRIPT_DIR/build_gcc.sh" "${@:1}"; then
  exit 1
fi
