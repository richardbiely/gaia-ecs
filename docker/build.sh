#!/bin/bash

if ! bash ./build_clang.sh "${@:1}"; then
  exit 1
fi

if ! bash ./build_gcc.sh "${@:1}"; then
  exit 1
fi
