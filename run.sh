#!/bin/bash
# Run one of the example exes (OpenGLCore, ControlTest) from any directory,
# building it first with -b. A convenience only: nothing depends on it, and
# the exes can be run directly too.

usage()
{
    echo "Usage: ./run.sh [-b] [debug|release] [asan] [opengl|controltest] [args...]"
    echo ""
    echo "  -b           build first (cmake --build, through run_docker.sh on a host)"
    echo "  debug        build_linux_x64_debug (default)"
    echo "  release      build_linux_x64_release"
    echo "  asan         the AddressSanitizer tree, build_linux_x64_debug_asan"
    echo "               (build_linux_x64_release_asan with release)"
    echo "  opengl       OpenGLCore, the interactive demo (default)"
    echo "  controltest  ControlTest"
    echo "  -h, --help   this text; after the exe name it goes to the exe instead,"
    echo "               e.g. ./run.sh controltest --help"
    echo ""
    echo "The first argument that is none of these words, and everything after it,"
    echo "is passed to the exe unchanged. The exe runs in the current directory, so"
    echo "relative paths (--shot, --screenshot, --resources) are relative to it."
    echo "By default the exe loads the resources/ copied next to it at build time;"
    echo "--resources resources (from the repo root) loads the live ones instead."
}

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

build=false
build_type="debug"
sanitize=""
target=""

while [ "$#" -gt 0 ]; do
    if [ "$1" = "-b" ]; then
        build=true
    elif [ "$1" = "debug" ]; then
        build_type="debug"
    elif [ "$1" = "release" ]; then
        build_type="release"
    elif [ "$1" = "asan" ]; then
        sanitize="asan"
    elif [ "$1" = "opengl" ]; then
        target="OpenGLCore"
    elif [ "$1" = "controltest" ]; then
        target="ControlTest"
    elif [ -z "$target" ] && { [ "$1" = "-h" ] || [ "$1" = "--help" ]; }; then
        usage
        exit 0
    else
        break
    fi
    shift
done

if [ -z "$target" ]; then
    target="OpenGLCore"
fi

# Named as generate.sh names it.
build_dir="build_linux_x64_${build_type}"
generate_args="linux"
if [ "$build_type" = "release" ]; then
    generate_args="$generate_args release"
fi
if [ "$sanitize" = "asan" ]; then
    build_dir="${build_dir}_asan"
    generate_args="$generate_args asan"
fi

exe="$root/$build_dir/examples/openglcore/$target"

# The build tooling lives in the container: VCPKG_ROOT is set only in there.
if [ -n "$VCPKG_ROOT" ]; then
    docker_prefix=""
else
    docker_prefix="./run_docker.sh "
fi

missing()
{
    echo "ERROR: $1 does not exist. Configure and build it, from $root:"
    echo "  ${docker_prefix}./generate.sh $generate_args"
    echo "  ${docker_prefix}cmake --build $build_dir -j"
    exit 1
}

if [ ! -d "$root/$build_dir" ]; then
    missing "$root/$build_dir"
fi

if [ "$build" = true ]; then
    if [ -n "$VCPKG_ROOT" ]; then
        cmake --build "$root/$build_dir" -j
    else
        # run_docker.sh mounts ${PWD} as the repo, so it runs from the root.
        (cd "$root" && ./run_docker.sh cmake --build "$build_dir" -j)
    fi
    status=$?
    if [ "$status" -ne 0 ]; then
        echo "ERROR: building $build_dir failed (exit $status), not running $target."
        exit "$status"
    fi
fi

if [ ! -x "$exe" ]; then
    missing "$exe"
fi

exec "$exe" "$@"
