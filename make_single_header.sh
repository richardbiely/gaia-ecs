#!/bin/bash

PATH_TO_AMALGAMATE_DIR="${@:1}"
PATH_TO_AMALGAMATE=${PATH_TO_AMALGAMATE_DIR}/amalgamate
rm -f ./single_include/gaia.h
${PATH_TO_AMALGAMATE} -i ./include -w "*.cpp;*.h;*.hpp;*.inl" ./include/gaia.h ./single_include/gaia.h