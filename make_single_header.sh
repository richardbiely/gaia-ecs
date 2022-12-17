#!/bin/bash

# Install quom via python script:
# pip install --user quom

# This turns Gaia-ECS into a single header library by generating "gaia.h"
# which you can simply drop into your project and start using.
quom ./include/gaia.h ./single_include/gaia.h -I ./include
