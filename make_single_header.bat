echo off

set "PATH_TO_AMALGAMATE_DIR=%1"
set "PATH_TO_AMALGAMATE=%PATH_TO_AMALGAMATE_DIR%/amalgamate"
del single_include/gaia.h
"%PATH_TO_AMALGAMATE%" -i include -w "*.cpp;*.h;*.hpp;*.inl" include/gaia.h single_include/gaia.h