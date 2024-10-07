FROM ubuntu:20.04

ENV LANG C.UTF-8
ARG DEBIAN_FRONTEND=noninteractive

CMD /bin/bash

RUN apt update && apt install -y --no-install-recommends \
    software-properties-common \
    clang \
    clang-format \
    clangd \
    llvm \
    g++ \
    gcc \
    gdb \
    make \
    cmake \
    tzdata \
    git \
    ninja-build \
    neovim \
    valgrind \
    python3.8-venv \
    openssh-server \
    && git clone --branch stable https://github.com/rui314/mold.git \
    && cd mold \
    && ./install-build-deps.sh \
    && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -B build \
    && cmake --build build -j$(nproc) \
    && cmake --build build --target install \
    && rm -rf /var/lib/apt/lists/* \
    && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && echo 'root:docker' | chpasswd

EXPOSE 22
ENTRYPOINT service ssh restart && bash
