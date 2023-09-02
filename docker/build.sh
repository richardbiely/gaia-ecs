#!/bin/bash

bash ./build_clang.sh ${@:1}
if [ $? -ne 0 ]; then
  exit 1
fi

bash ./build_gcc.sh ${@:1}
if [ $? -ne 0 ]; then
  exit 1
fi
