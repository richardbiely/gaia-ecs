FROM amd64/ubuntu:22.04

ENV LANG=C.UTF-8
ARG DEBIAN_FRONTEND=noninteractive

####################################################################################################
# Use bash for shell
####################################################################################################
CMD ["/bin/bash"]

####################################################################################################
# Install basic dependencies
####################################################################################################
RUN apt update && apt install -y --no-install-recommends \
    software-properties-common \
    clang \
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
    openssh-server

####################################################################################################
# Purge the apt list
####################################################################################################
RUN rm -rf /var/lib/apt/lists/*

####################################################################################################
# Set up ssh
####################################################################################################
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && echo 'root:docker' | chpasswd

EXPOSE 22
ENTRYPOINT ["sh", "-c", "service ssh restart && exec bash"]
