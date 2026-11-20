#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Builds the gweni Python wheel (pyproject.toml, scikit-build-core) in the
#  build container, from any directory:
#
#      ./tools/build_wheel.sh                       # dist/gweni-<ver>-cp312-abi3-linux_x86_64.whl
#      ./tools/build_wheel.sh --out /tmp/wheels
#      ./tools/build_wheel.sh --vcpkg-installed build_linux_x64_release/vcpkg_installed --offline
#
#  The configuration is ./generate.sh's (`./generate.sh linux release
#  --print-cmake-args`: the vcpkg toolchain, the chainloaded linux toolchain,
#  the triplet, the renderer options, Python_EXECUTABLE), with Vulkan, the
#  examples and the tests off (Python reaches no Vulkan in v1, and the examples
#  and tests would fetch the lazy CJK face) and GWENI_BUILD_PYTHON on, so the
#  wheel is the desktop GL build. It is built with `uv build --wheel
#  --no-build-isolation` on GWENI_PYTHON (the image's CPython 3.12 venv, which
#  holds the pinned scikit-build-core and nanobind), in a persistent build
#  directory, so a second build is incremental.
#
#  Each mode has a build directory of its own (vcpkg refuses to switch
#  VCPKG_MANIFEST_MODE in a configured directory, and the cache keeps the
#  package paths of the vcpkg_installed it was configured with), and the
#  directory records what it was configured for (.gweni-wheel-config): a run
#  whose mode or vcpkg_installed differs from the record clears it first.
#  A directory is cleared only if it is one of these: recorded, or
#  scikit-build-core's (SKBUILD in its CMake cache); any other configured
#  tree, ./generate.sh's own say, is refused and left as it is.
#
#  Options:
#    --out DIR              where the wheel goes (default dist/ in the repo,
#                           git-ignored)
#    --build-dir DIR        scikit-build-core's build directory (default
#                           build_linux_x64_release_wheel in the repo, or
#                           build_linux_x64_release_wheel_installed with
#                           --vcpkg-installed)
#    --vcpkg-installed DIR  use that vcpkg_installed (a configured tree's) with
#                           no manifest install, so vcpkg builds and fetches
#                           nothing (python_wheel does, with the release tree's)
#    --offline              uv touches no network (with --vcpkg-installed, the
#                           whole build is offline)
#    --print-build-dir      print the build directory this run would use and
#                           exit, touching nothing
#    --dry-run              prepare the build directory (clear it, record the
#                           configuration), print `build-dir=DIR` and the
#                           configure's arguments one a line, and exit before
#                           the build (python_wheel_build_dirs)
#    --help                 this text
#
#  The wheel is linux_x86_64, which PyPI refuses: tools/repair_wheel.py makes
#  it manylinux_2_35_x86_64 and checks it. The build holds no lock.
#
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$repo/dist"
build_dir=""
vcpkg_installed=""
offline=()
print_build_dir=0
dry_run=0

usage() { sed -n '7,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^#  \{0,1\}//; s/^#$//'; }

while [ $# -gt 0 ]; do
    case "$1" in
        --out) out="$(realpath -m "$2")"; shift 2 ;;
        --build-dir) build_dir="$(realpath -m "$2")"; shift 2 ;;
        --vcpkg-installed) vcpkg_installed="$(realpath "$2")"; shift 2 ;;
        --offline) offline=(--offline); shift ;;
        --print-build-dir) print_build_dir=1; shift ;;
        --dry-run) dry_run=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) echo "build_wheel.sh: unknown argument $1 (--help)" >&2; exit 2 ;;
    esac
done

if [ -z "$build_dir" ]; then
    if [ -n "$vcpkg_installed" ]; then
        build_dir="$repo/build_linux_x64_release_wheel_installed"
    else
        build_dir="$repo/build_linux_x64_release_wheel"
    fi
fi
if [ "$print_build_dir" = 1 ]; then
    echo "$build_dir"
    exit 0
fi

# The configuration the build directory is for; one configured for another is
# cleared, but only one of ours: the record, or with none (a directory from
# before it) scikit-build-core's CMake cache of this checkout (SKBUILD in it).
# Any other configured tree, ./generate.sh's release tree given as --build-dir
# say, is refused and left as it is, --dry-run or not.
if [ -n "$vcpkg_installed" ]; then
    config="installed $vcpkg_installed"
else
    config="manifest"
fi
record="$build_dir/.gweni-wheel-config"
if [ -d "$build_dir" ] && [ -n "$(ls -A "$build_dir")" ] && [ "$(cat "$record" 2>/dev/null)" != "$config" ]; then
    if [ -f "$record" ] || { grep -qxF "CMAKE_HOME_DIRECTORY:INTERNAL=$repo" "$build_dir/CMakeCache.txt" 2>/dev/null \
            && grep -q '^SKBUILD:STRING=' "$build_dir/CMakeCache.txt"; }; then
        echo "build_wheel.sh: $build_dir was configured for $(cat "$record" 2>/dev/null || echo 'another vcpkg setup'), not $config: clearing it"
        rm -rf "$build_dir"
    else
        echo "build_wheel.sh: $build_dir is not a wheel build directory of this checkout; give another --build-dir" >&2
        exit 1
    fi
fi
mkdir -p "$build_dir"
echo "$config" > "$record"

python="${GWENI_PYTHON:-/opt/python/venv/bin/python}"
if [ ! -x "$python" ]; then
    echo "build_wheel.sh: no Python at $python (GWENI_PYTHON); from a host, ./run_docker.sh -r rebuilds the image with it" >&2
    exit 1
fi
if [ "$dry_run" = 0 ] && ! command -v uv >/dev/null; then
    echo "build_wheel.sh: no uv on PATH (the image's, Dockerfile)" >&2
    exit 1
fi

# ./generate.sh decides the configuration; its pass-through -D come last and win
if ! printed="$(GWENI_PYTHON="$python" "$repo/generate.sh" linux release --print-cmake-args \
    -DGWENI_RENDER_VULKAN=OFF -DGWENI_BUILD_EXAMPLES=OFF -DGWENI_BUILD_TESTS=OFF -DGWENI_BUILD_PYTHON=ON)"; then
    echo "build_wheel.sh: ./generate.sh --print-cmake-args failed:" >&2
    printf '%s\n' "$printed" >&2
    exit 1
fi
mapfile -t args <<< "$printed"
if [ -z "$printed" ] || [ "${#args[@]}" -eq 0 ]; then
    echo "build_wheel.sh: ./generate.sh --print-cmake-args printed nothing" >&2
    exit 1
fi
# The manifest mode and the installed tree are passed on every run, either
# way, as the record above says
if [ -n "$vcpkg_installed" ]; then
    args+=("-DVCPKG_MANIFEST_MODE=OFF" "-DVCPKG_INSTALLED_DIR=$vcpkg_installed")
else
    args+=("-DVCPKG_MANIFEST_MODE=ON" "-DVCPKG_INSTALLED_DIR=$build_dir/vcpkg_installed")
fi
for arg in "${args[@]}"; do
    if [[ $arg == *";"* ]]; then
        echo "build_wheel.sh: a configure argument holds a ';', which cmake.args cannot pass: $arg" >&2
        exit 1
    fi
done
joined="$(IFS=';'; echo "${args[*]}")"
if [ "$dry_run" = 1 ]; then
    echo "build-dir=$build_dir"
    printf '%s\n' "${args[@]}"
    exit 0
fi

export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc)}"
mkdir -p "$out"
cd "$repo"
uv build --wheel --no-build-isolation "${offline[@]}" --python "$python" --out-dir "$out" \
    -C "build-dir=$build_dir" -C "cmake.args=$joined" "$repo"
