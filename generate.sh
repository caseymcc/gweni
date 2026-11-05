#!/bin/bash

arch="x64"
build_type="debug"
platform="linux"
cmake_options=""
cmake_custom_options=""

for var in "$@"; do
    if [ "$var" = "x64" ]; then
        arch="x64"
    fi
    if [ "$var" = "arm" ]; then
        arch="arm32"
    fi
    if [ "$var" = "arm32" ]; then
        arch="arm32"
    fi
    if [ "$var" = "arm64" ]; then
        arch="arm64"
    fi
    if [ "$var" = "linux" ]; then
        platform="linux"
    fi
    if [ "$var" = "android" ]; then
        platform="android"
    fi
    if [ "$var" = "release" ]; then
        build_type="release"
    fi
    if [[ $var = -D* ]]; then
        cmake_custom_options+="$var "
    fi
done

if [ -z "$VCPKG_ROOT" ]; then
    echo "ERROR: VCPKG_ROOT is not set."
    echo "Dependencies are managed by vcpkg inside the Docker container."
    echo "Run this through Docker, e.g.:  ./run_docker.sh ./generate.sh $@"
    exit 1
fi

echo "**Building $platform $arch for $build_type"

if [ "$build_type" = "debug" ]; then
    cmake_options="$cmake_options \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_DEBUG_POSTFIX=d"
else
    cmake_options="$cmake_options \
        -DCMAKE_BUILD_TYPE=Release"
fi

if [ "$platform" = "android" ]; then

    anrdoid_arch="arm64-v8a"

    if [ "$arch" = "arm32" ]; then
        anrdoid_arch="armv7a"
    fi

    cmake_options=""
fi

if [ "$platform" = "linux" ]; then

    # vcpkg provides the dependencies; chainload our linux toolchain so its
    # settings (e.g. OpenGL GLVND preference) still apply.
    cmake_options="$cmake_options \
        -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$(pwd)/cmake/linux_toolchain.cmake \
        -DVCPKG_TARGET_TRIPLET=x64-linux \
        -DGWENI_RENDER_SDL2=OFF \
        -DGWENI_RENDER_OPENGL=OFF \
        -DGWENI_RENDER_OPENGL_CORE=ON"
fi

build_dir="build_${platform}_${arch}_${build_type}"

cmake -H. -B$build_dir \
    $cmake_options $cmake_custom_options