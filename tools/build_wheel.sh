#!/usr/bin/env bash
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
#  Options:
#    --out DIR              where the wheel goes (default dist/ in the repo,
#                           git-ignored)
#    --build-dir DIR        scikit-build-core's build directory (default
#                           build_linux_x64_release_wheel in the repo)
#    --vcpkg-installed DIR  use that vcpkg_installed (a configured tree's) with
#                           no manifest install, so vcpkg builds and fetches
#                           nothing (python_wheel does, with the release tree's)
#    --offline              uv touches no network (with --vcpkg-installed, the
#                           whole build is offline)
#    --help                 this text
#
#  The wheel is linux_x86_64, not yet manylinux: the auditwheel repair is the
#  publishing task's (#858). The build holds no lock.
#
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$repo/dist"
build_dir="$repo/build_linux_x64_release_wheel"
vcpkg_installed=""
offline=()

usage() { sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^#  \{0,1\}//; s/^#$//'; }

while [ $# -gt 0 ]; do
    case "$1" in
        --out) out="$(realpath -m "$2")"; shift 2 ;;
        --build-dir) build_dir="$(realpath -m "$2")"; shift 2 ;;
        --vcpkg-installed) vcpkg_installed="$(realpath "$2")"; shift 2 ;;
        --offline) offline=(--offline); shift ;;
        --help|-h) usage; exit 0 ;;
        *) echo "build_wheel.sh: unknown argument $1 (--help)" >&2; exit 2 ;;
    esac
done

python="${GWENI_PYTHON:-/opt/python/venv/bin/python}"
if [ ! -x "$python" ]; then
    echo "build_wheel.sh: no Python at $python (GWENI_PYTHON); from a host, ./run_docker.sh -r rebuilds the image with it" >&2
    exit 1
fi
if ! command -v uv >/dev/null; then
    echo "build_wheel.sh: no uv on PATH (the image's, Dockerfile)" >&2
    exit 1
fi

# ./generate.sh decides the configuration; its pass-through -D come last and win
mapfile -t args < <(GWENI_PYTHON="$python" "$repo/generate.sh" linux release --print-cmake-args \
    -DGWENI_RENDER_VULKAN=OFF -DGWENI_BUILD_EXAMPLES=OFF -DGWENI_BUILD_TESTS=OFF -DGWENI_BUILD_PYTHON=ON)
if [ "${#args[@]}" -eq 0 ]; then
    echo "build_wheel.sh: ./generate.sh --print-cmake-args printed nothing" >&2
    exit 1
fi
if [ -n "$vcpkg_installed" ]; then
    args+=("-DVCPKG_MANIFEST_MODE=OFF" "-DVCPKG_INSTALLED_DIR=$vcpkg_installed")
fi
for arg in "${args[@]}"; do
    if [[ $arg == *";"* ]]; then
        echo "build_wheel.sh: a configure argument holds a ';', which cmake.args cannot pass: $arg" >&2
        exit 1
    fi
done
joined="$(IFS=';'; echo "${args[*]}")"

export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc)}"
mkdir -p "$out"
cd "$repo"
uv build --wheel --no-build-isolation "${offline[@]}" --python "$python" --out-dir "$out" \
    -C "build-dir=$build_dir" -C "cmake.args=$joined" "$repo"
