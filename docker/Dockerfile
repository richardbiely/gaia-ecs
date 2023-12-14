FROM ubuntu:20.04

ENV LANG C.UTF-8
ARG DEBIAN_FRONTEND=noninteractive

CMD /bin/bash

RUN apt update && apt install -y --no-install-recommends \
    software-properties-common \
    clang \
    g++ \
    gcc \
    gdb \
    make \
    cmake \
    tzdata \
    git \
    ninja-build \
    dos2unix \
    neovim \
    valgrind \
    && rm -rf /var/lib/apt/lists/*
