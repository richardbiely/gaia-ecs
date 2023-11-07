#!/bin/bash

PATH_TO_QUOM_DIR="${@:1}"
PATH_TO_PIP_EVN="${PATH_TO_QUOM_DIR}/../../../pip_venv"
PATH_TO_QUOM=$(which quom)

# If quom is not found right away, try searching in the virtual environment
if [ -z "$PATH_TO_QUOM" ]; then
  PATH_TO_QUOM=$(which "${PATH_TO_PIP_EVN}/bin/quom")
fi

if [ -z "${PATH_TO_QUOM}" ]; then
  PATH_TO_QUOM="${PATH_TO_PIP_EVN}/bin/quom"

  # Still not found. Try installing quom into the virtual environment.
  # This requires python3 to be available.
  echo "Creating virtual environment for quom to ${PATH_TO_PIP_EVN}"
  python3 -m venv "${PATH_TO_PIP_EVN}"
  echo "Installing quom to ${PATH_TO_PIP_EVN}/bin"
  "${PATH_TO_PIP_EVN}/bin/pip" install "${PATH_TO_QUOM_DIR}"
  chmod +x "${PATH_TO_QUOM}"
fi

# This turns Gaia-ECS into a single header library by generating "gaia.h"
# which you can simply drop into your project and start using.
echo "Generating ./single_include/gaia.h"
${PATH_TO_QUOM} ./include/gaia.h ./single_include/gaia.h -I ./include