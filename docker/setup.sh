#!/bin/bash

imagename="gaiaecs-linux-builder"
imagename_tmp=${imagename}"tmp"

# Stop running containers on the image and remove them
docker stop -t 0 ${imagename}

# Make sure the container is removed
docker rm ${imagename}

# Build the image if necessary. Decide what docker file to use based on the detected platform
CURRENT_PLATFORM=$(uname -p)
if [[ "$CURRENT_PLATFORM" = "i386|i686|x86_64" ]]; then
  # Use specialized version for amd64 because the Intel compiler is not compatible with anything else
  docker build --file amd64.Dockerfile --tag ${imagename} .
else
  # Generic docker file
  docker build --file Dockerfile --tag ${imagename} .
fi

# Make sure scripts are in unix format
dos2unix ./build.sh
dos2unix ./build_clang.sh
dos2unix ./build_clang_cachegrind.sh
dos2unix ./build_gcc.sh

# Start the container
docker volume create ${imagename_tmp}
docker run -p 2022:22 --rm --interactive --tty --privileged --name ${imagename} --mount type=volume,source=${imagename_tmp},target=/work-output --mount type=bind,source=$(pwd)/..,target=/gaia-ecs --workdir /gaia-ecs/docker ${imagename} bash
