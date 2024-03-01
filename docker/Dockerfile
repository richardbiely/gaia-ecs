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
    python3.8-venv \
    openssh-server \
    && rm -rf /var/lib/apt/lists/* \
    && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && echo 'root:docker' | chpasswd

EXPOSE 22
ENTRYPOINT service ssh restart && bash
