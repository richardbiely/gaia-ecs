#!/bin/bash

PATH_TO_QUOM_DIR="${@:1}"

# Install quom via python script if necessary.
# This requires pip to be available.
HAS_QUOM=$(which quom)
if [ -z "$HAS_QUOM" ]; then
  echo "Installing quim to ${PATH_TO_QUOM_DIR}"
  pip install ${PATH_TO_QUOM_DIR}
fi

# This turns Gaia-ECS into a single header library by generating "gaia.h"
# which you can simply drop into your project and start using.
echo "Generating ./single_include/gaia.h"
quom ./include/gaia.h ./single_include/gaia.h -I ./include
