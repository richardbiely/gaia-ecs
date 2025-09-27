#!/bin/bash

imagename="gaiaecs-linux-builder"
imagename_tmp="${imagename}tmp"

# Stop running container if it exists
docker stop -t 0 ${imagename} 2>/dev/null || true
docker rm ${imagename} 2>/dev/null || true

# Decide which Dockerfile to use based on CPU architecture
CURRENT_PLATFORM=$(uname -p)
if [[ "$CURRENT_PLATFORM" =~ i[3-6]86|x86_64 ]]; then
  dockerfile="amd64.Dockerfile"
else
  dockerfile="Dockerfile"
fi

# Build the image
docker build --file "${dockerfile}" --tag "${imagename}" .

# Create a volume
docker volume create ${imagename_tmp} 2>/dev/null || true

# Run the container
docker run -p 2022:22 \
  --rm \
  -it \
  --privileged \
  --name "${imagename}" \
  --mount type=volume,source=${imagename_tmp},target=/work-output \
  --mount type=bind,source=$(pwd)/..,target=/gaia-ecs \
  --workdir /gaia-ecs/docker \
  "${imagename}" bash