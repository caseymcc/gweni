#!/usr/bin/env bash
#
#  Builds, tests and stages the hosted site, as the GitHub Pages workflow
#  (.github/workflows/pages.yml) runs it: every step of the workflow's build is
#  here, so the YAML holds no build logic and a run can be repeated by hand.
#
#  Inside the build container (it needs vcpkg and emsdk), from any directory:
#
#      ./tools/ci_site.sh                 # build, test, stage site/, check README
#      ./tools/ci_site.sh --out /tmp/site # stage the site elsewhere
#
#  Options:
#    --out DIR   where tools/build_site.sh stages the site (default site/ in the
#                repo, which is git-ignored, and what the workflow uploads).
#    --help      this text.
#
#  In order, stopping at the first failure:
#    1. the native GLES release tree, build_linux_x64_release_gles/, with
#       Vulkan off (-DGWENI_RENDER_VULKAN=OFF, which leaves out the vcpkg
#       feature too): only its ControlTest and OpenGLCore, which the web tests'
#       web_*_native comparisons run as their reference. Vulkan is off because
#       nothing here uses it, and its vcpkg ports (glslang, the validation
#       layers) would double a cold dependency build. A tree configured with
#       Vulkan on is reconfigured without it.
#    2. the release web tree, build_web_wasm32_release/, whole: the pages, the
#       wasm unit tests and gweni_unittests.html.
#    3. ctest -L web in it: the browser tests, web_site_smoke and the
#       web_*_native comparisons included. Their native runs take the render
#       lock through mm when mm is on PATH, and run unlocked (with one line
#       saying so) where it is not, as on a GitHub runner.
#    4. tools/build_site.sh --out DIR: the site, and its size against the
#       budget.
#    5. tools/check_site_links.py: every live-site link in README.md is a page
#       of the staged site.
#
#  It takes no lock itself: building takes none, and the tests take their own.
#
#  Every cmake --build runs at most CMAKE_BUILD_PARALLEL_LEVEL jobs, nproc
#  unless the environment sets it: a bare -j is make's unlimited jobs, which ran
#  the 4-core, 16 GB GitHub runner out of memory (#579).

set -euo pipefail

: "${CMAKE_BUILD_PARALLEL_LEVEL:=$(nproc)}"
export CMAKE_BUILD_PARALLEL_LEVEL

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$repo/site"

usage()
{
    sed -n '2,/^$/{s/^# \{0,2\}//;p}' "${BASH_SOURCE[0]}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out)
            [ $# -ge 2 ] || { echo "ci_site.sh: --out needs a directory" >&2; exit 1; }
            out="$(realpath -m "$2")"; shift 2 ;;
        --help|-h)
            usage; exit 0 ;;
        *)
            echo "ci_site.sh: unknown argument: $1" >&2
            usage >&2
            exit 1 ;;
    esac
done

if [ -z "${VCPKG_ROOT:-}" ] || [ -z "${EMSDK:-}" ]; then
    echo "ci_site.sh: VCPKG_ROOT or EMSDK is not set: run it inside the build container (./run_docker.sh ./tools/ci_site.sh)" >&2
    exit 1
fi

step()
{
    echo
    echo "ci_site.sh: == $*"
}

# generate.sh names its trees from the cwd
cd "$repo"

step "1/5 native GLES release tree (the web tests' reference)"
./generate.sh linux gles release -DGWENI_RENDER_VULKAN=OFF
cmake --build build_linux_x64_release_gles --target ControlTest OpenGLCore

step "2/5 release web tree"
./generate.sh web release
cmake --build build_web_wasm32_release

step "3/5 the web tests"
ctest --test-dir build_web_wasm32_release -L web --output-on-failure

step "4/5 stage the site in $out"
"$repo/tools/build_site.sh" --out "$out"

step "5/5 README.md's live links"
python3 -I "$repo/tools/check_site_links.py" "$repo/README.md" "$out"

echo
echo "ci_site.sh: done, the site is in $out"
