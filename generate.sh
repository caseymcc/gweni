#!/bin/bash

arch="x64"
build_type="debug"
platform="linux"
cmake_options=""
cmake_custom_options=""
sanitize=""
profile=""
manifest_features=""
vulkan="ON"

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
    if [ "$var" = "asan" ]; then
        sanitize="asan"
    fi
    if [ "$var" = "profile" ]; then
        profile="profile"
    fi
    if [[ $var = -DVCPKG_MANIFEST_FEATURES=* || $var = -DVCPKG_MANIFEST_FEATURES:*=* ]]; then
        # merged with the features this script adds, below
        manifest_features="${var#*=}"
    elif [[ $var = -D* ]]; then
        cmake_custom_options+="$var "
    fi
    if [[ $var = -DGWENI_RENDER_VULKAN=* || $var = -DGWENI_RENDER_VULKAN:*=* ]]; then
        vulkan="${var#*=}"
    fi
done

if [ -z "$VCPKG_ROOT" ]; then
    echo "ERROR: VCPKG_ROOT is not set."
    echo "Dependencies are managed by vcpkg inside the Docker container."
    echo "Run this through Docker, e.g.:  ./run_docker.sh ./generate.sh $@"
    exit 1
fi

if [ -n "$profile" ]; then
    # The profile tree is its own build type (optimised, with debug info and
    # frame pointers), so it does not combine with release or asan.
    if [ "$build_type" = "release" ] || [ -n "$sanitize" ]; then
        echo "ERROR: profile does not combine with release or asan; use ./generate.sh linux profile"
        exit 1
    fi
    build_type="profile"
fi

echo "**Building $platform $arch for $build_type"

if [ "$build_type" = "debug" ]; then
    cmake_options="$cmake_options \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_DEBUG_POSTFIX=d"
elif [ "$build_type" = "profile" ]; then
    # callgrind and other profilers: -O2 -g, and frame pointers in everything
    # built here (GWENI_FRAME_POINTERS), deps/ included
    cmake_options="$cmake_options \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DGWENI_FRAME_POINTERS=ON"
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
        -DGWENI_RENDER_OPENGL_CORE=ON \
        -DGWENI_RENDER_VULKAN=ON"

    # The Vulkan renderer is on in every tree configured here, so its tests run
    # in every ctest (CMake's own default is OFF). A -DGWENI_RENDER_VULKAN=OFF
    # passed through comes after the ON above and wins, and then the vcpkg
    # feature is left out too. Features passed with -DVCPKG_MANIFEST_FEATURES
    # are merged with it.
    case "${vulkan^^}" in
        OFF|0|FALSE|NO|N) ;;
        *) manifest_features="vulkan${manifest_features:+;$manifest_features}" ;;
    esac
fi

# Always passed, empty or not, so a feature cached by an earlier configure of
# the same tree does not stay on.
cmake_custom_options+="-DVCPKG_MANIFEST_FEATURES=$manifest_features"

build_dir="build_${platform}_${arch}_${build_type}"

if [ "$sanitize" = "asan" ]; then
    # AddressSanitizer tree, next to the plain one: build_linux_x64_debug_asan
    cmake_options="$cmake_options -DGWENI_SANITIZE=address"
    build_dir="${build_dir}_asan"
fi

cmake -H. -B$build_dir \
    $cmake_options $cmake_custom_options