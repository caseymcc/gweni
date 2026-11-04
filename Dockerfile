FROM ubuntu:22.04

ARG HOST_HOME_DIR
ARG USER_NAME=developer
ARG USER_ID
ARG GROUP_ID
ARG OS=ubuntu1804
ARG ARCH=x86_64

ENV DEBIAN_FRONTEND=noninteractive

#setup user to host user id and groupid
RUN if [ ${USER_ID:-0} -ne 0 ] && [ ${GROUP_ID:-0} -ne 0 ]; then \
    if id $USER_NAME >/dev/null 2>&1; then userdel -f $USER_NAME; fi &&\
    if getent group $USER_NAME ; then groupdel $USER_NAME; fi &&\
    groupadd -g ${GROUP_ID} $USER_NAME &&\
    useradd -l -u ${USER_ID} -g $USER_NAME $USER_NAME &&\
    install -d -m 0755 -o $USER_NAME -g $USER_NAME /home/$USER_NAME \
;fi

WORKDIR /root

RUN echo 'root:toor' | chpasswd

# Build toolchain + vcpkg prerequisites + system libraries that the vcpkg ports
# (glfw3, glew, freetype, ...) need to build from source on Linux.
RUN apt-get update && \
    apt-get install -y \
        apt-utils \
        software-properties-common \
        git \
        ca-certificates \
        gpg \
        wget \
        curl \
        zip \
        unzip \
        tar \
        usbutils \
        build-essential \
        pkg-config \
        ninja-build \
        autoconf \
        automake \
        libtool \
        python3 \
        python3-pip \
        python3-jinja2 \
        bison \
        flex \
        xorg-dev \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libxmu-dev \
        libxrandr-dev

#add kitware cmake repo to get latest, need at least 3.14
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN add-apt-repository 'deb https://apt.kitware.com/ubuntu/ jammy main'
RUN apt-get update && \
    apt-get install kitware-archive-keyring && \
    rm /etc/apt/trusted.gpg.d/kitware.gpg

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        cmake

#clean up apt files, reduce docker size
RUN rm -rf /var/lib/apt/lists/*

# Install vcpkg into the image. Dependencies are resolved from vcpkg.json at
# configure time. The binary cache and downloads are persisted via a volume
# mounted at /vcpkg-cache at run time; VCPKG_DEFAULT_BINARY_CACHE and
# VCPKG_DOWNLOADS are injected then (see run_docker.sh) rather than here, since
# the volume does not exist during the image build.
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=${VCPKG_ROOT}:${PATH}

RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} && \
    ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics && \
    chown -R ${USER_NAME}:${USER_NAME} ${VCPKG_ROOT}

#set owner of everything in home to user
RUN chown -R $USER_NAME:$USER_NAME /home/$USER_NAME/

#swith to user with host id/groupid
USER $USER_NAME

WORKDIR /home/$USER_NAME/gweni/

ENTRYPOINT ["/bin/bash"]
