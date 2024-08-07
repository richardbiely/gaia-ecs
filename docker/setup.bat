echo off

set imagename=gaiaecs-linux-builder
set imagename_tmp=%imagename%-tmp

@REM Stop running containers on the image and remove them
docker stop -t 0 %imagename%

@REM Make sure the container is removed
docker rm %imagename%

@REM Build the image if necessary. Decide what docker file to use based on the detected platform
set currach=%PROCESSOR_ARCHITECTURE%

if "%currach%"=="ARM64" (
  @REM Generic docker file
  docker build --file Dockerfile --tag %imagename% .
) else if "%currach%"=="AMD64" (
  @REM Use specialized version for amd64 because the Intel compiler is not compatible with anything else
  docker build --file amd64.Dockerfile --tag %imagename% .
) else if "%currach%"=="x86" (
  @REM Use specialized version for amd64 because the Intel compiler is not compatible with anything else
  docker build --file amd64.Dockerfile --tag %imagename% .
) else (
  echo The current platform architecture is unknown: %currach%
  exit 0
)

@REM Make sure scripts are in unix format
dos2unix ./build.sh
dos2unix ./build_clang.sh
dos2unix ./build_clang_cachegrind.sh
dos2unix ./build_gcc.sh

@REM Start the container
docker volume create %imagename_tmp%
set currdir=%~dp0
docker run -p 2022:22 --rm --interactive --tty --privileged ^
  --name %imagename% ^
  --mount type=volume,source=%imagename_tmp%,target=/work-output ^
  --mount type=bind,source=%currdir%..,target=/gaia-ecs ^
  --workdir /gaia-ecs/docker %imagename% bash

pause