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
RUN wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
RUN APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=1 apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
RUN rm GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
RUN echo "deb https://apt.repos.intel.com/oneapi all main" | tee /etc/apt/sources.list.d/oneAPI.list
RUN add-apt-repository "deb https://apt.repos.intel.com/oneapi all main"
RUN intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic

RUN rm -rf /var/lib/apt/lists/*