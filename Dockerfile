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
        autoconf-archive \
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

# Profiling (./generate.sh linux profile, gweni_bench under callgrind):
# valgrind (callgrind, callgrind_annotate and <valgrind/callgrind.h>), graphviz
# (dot, for the call-graph SVG) and gprof2dot, which Ubuntu 22.04 does not
# package, from PyPI at a pinned version.
RUN apt-get update && \
    apt-get install -y \
        valgrind \
        graphviz && \
    pip3 install --no-cache-dir gprof2dot==2025.4.14

# Vulkan (GWENI_RENDER_VULKAN, on in every tree ./generate.sh configures): the
# loader (libvulkan1, which volk and GLFW open at run time), Mesa's lavapipe CPU
# driver (mesa-vulkan-drivers; Mesa 23.2 here, Vulkan 1.3) that the Vulkan
# tests run on, as the GL tests run on llvmpipe, and Xvfb for the windowed
# Vulkan test. libwayland-dev is for vcpkg's vulkan-validationlayers port,
# whose build needs wayland-client's headers. The layer itself, the headers,
# volk and glslang come from vcpkg (the manifest feature "vulkan").
RUN apt-get update && \
    apt-get install -y \
        libvulkan1 \
        mesa-vulkan-drivers \
        libwayland-dev \
        xvfb

# The web tree (./generate.sh web, build_web_wasm32_<debug|release>/) and its
# browser tests, the versions pinned here and nowhere else:
# - emsdk at /opt/emsdk: Emscripten, its clang, and the node it bundles, which
#   runs the wasm tests and Playwright (the image has no other node). The
#   contrib.glfw3 port (emscripten-glfw) is fetched into the cache now, so a
#   build needs no network. /opt/emsdk/node/current links to the bundled node,
#   so the ENV lines below need not name its version.
# - Playwright (npm) at /opt/playwright, found through NODE_PATH, with the
#   Chromium headless shell it pins in PLAYWRIGHT_BROWSERS_PATH and that
#   shell's apt dependencies (install-deps, before the apt lists are removed).
#   tests/web/probe.mjs (CTest web_probe) checks that it gives WebGL 2.
# About 2.1 GB more image: emsdk 1.7 GB, Playwright and its headless shell
# 0.3 GB, the shell's 21 apt packages (fonts, mostly) 0.07 GB.
ARG EMSDK_VERSION=6.0.13
ARG PLAYWRIGHT_VERSION=1.65.0
ENV EMSDK=/opt/emsdk
ENV EMSDK_NODE=${EMSDK}/node/current/bin/node
ENV PATH=${EMSDK}:${EMSDK}/upstream/emscripten:${EMSDK}/node/current/bin:${PATH}
ENV NODE_PATH=/opt/playwright/node_modules
ENV PLAYWRIGHT_BROWSERS_PATH=/opt/playwright/browsers
RUN git clone --depth 1 --branch ${EMSDK_VERSION} https://github.com/emscripten-core/emsdk.git ${EMSDK} && \
    ${EMSDK}/emsdk install ${EMSDK_VERSION} && \
    ${EMSDK}/emsdk activate ${EMSDK_VERSION} && \
    ln -s $(dirname $(dirname $(ls ${EMSDK}/node/*/bin/node | head -1))) ${EMSDK}/node/current && \
    rm -rf ${EMSDK}/downloads ${EMSDK}/.git && \
    embuilder build contrib.glfw3 && \
    mkdir -p /opt/playwright && \
    npm install --prefix /opt/playwright --no-audit --no-fund playwright@${PLAYWRIGHT_VERSION} && \
    /opt/playwright/node_modules/.bin/playwright install chromium-headless-shell && \
    apt-get update && \
    /opt/playwright/node_modules/.bin/playwright install-deps chromium-headless-shell && \
    chown -R ${USER_NAME}:${USER_NAME} ${EMSDK} /opt/playwright

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

# Render in software only: Mesa llvmpipe for GL/EGL and lavapipe for Vulkan,
# which the samples' bytes (docs/images/, docs_samples) depend on. A host may
# pass its GPU (/dev/dri) into the container, and without these a Mesa that
# knows that GPU would pick its hardware driver instead. VK_DRIVER_FILES is
# what newer loaders read, VK_ICD_FILENAMES what this one (1.3.204) does. Kept
# after the last RUN, so changing them rebuilds no layer.
ENV LIBGL_ALWAYS_SOFTWARE=1
ENV VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
ENV VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json

#swith to user with host id/groupid
USER $USER_NAME

WORKDIR /home/$USER_NAME/gweni/

ENTRYPOINT ["/bin/bash"]
