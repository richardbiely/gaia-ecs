FROM amd64/ubuntu:20.04

ENV LANG C.UTF-8
ARG DEBIAN_FRONTEND=noninteractive

CMD /bin/bash

RUN apt update && apt install -y --no-install-recommends \
    software-properties-common \
    clang \
    g++-7 \
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
    wget \
    gpg-agent

# Intel compiler is only compatible with x86 architecture
RUN wget https://registrationcenter-download.intel.com/akdlm/irc_nas/18673/l_BaseKit_p_2022.2.0.262_offline.sh && \
    chmod +x ./l_BaseKit_p_2022.2.0.262_offline.s && \
    ./l_BaseKit_p_2022.2.0.262_offline.sh

RUN rm -rf /var/lib/apt/lists/*