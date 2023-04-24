#!/bin/bash

imagename="gaiaecs-linux-builder"
imagename_tmp=${imagename}"tmp"

# stop running containers on the image and remove them
docker stop -t 0 ${imagename}

# make sure the container is removed
docker rm ${imagename}

# build the image if necessary
docker build --tag ${imagename} .

# start the container
docker volume create ${imagename_tmp}
docker run --rm --interactive --tty --privileged --name ${imagename} --mount type=volume,source=${imagename_tmp},target=/work-output --mount type=bind,source=$(pwd)/..,target=/gaia-ecs --workdir /gaia-ecs/docker ${imagename} bash
