#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  python_wheel_build_dirs (python/tests/tests.cmake, no gpu label, a second):
#  tools/build_wheel.sh's two modes never share a configured build directory,
#  so a plain build after python_wheel (and the reverse) configures, where
#  vcpkg refused a VCPKG_MANIFEST_MODE change in one directory (review 2 of
#  #857). Through --print-build-dir and --dry-run, which prepare the directory
#  and stop before the build:
#
#  1. the default directories of the plain and --vcpkg-installed modes differ;
#  2. a directory is recorded for the mode it is prepared for, with the
#     manifest mode and vcpkg_installed passed to the configure;
#  3. a directory recorded for one mode, or for another vcpkg_installed, is
#     cleared before the other uses it, and one recorded for the same is kept;
#  4. a directory of this checkout's from before the record (a CMake cache, no
#     record) is cleared, and one that is not a build directory of ours is
#     refused (exit 1) and left as it was.
#
#  GWENI_PYTHON names the 3.12 interpreter (the image's venv by default).
#
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
script="$repo/tools/build_wheel.sh"
scratch="$(mktemp -d /tmp/gweni-wheel-dirs.XXXXXX)"
fail() { echo "python_wheel_build_dirs: FAIL: $*" >&2; echo "(scratch kept: $scratch)" >&2; exit 1; }

installed_a="$scratch/installed_a"
installed_b="$scratch/installed_b"
mkdir -p "$installed_a" "$installed_b"

# 1. the defaults
plain_default="$("$script" --print-build-dir)"
installed_default="$("$script" --vcpkg-installed "$installed_a" --print-build-dir)"
[ "$plain_default" = "$repo/build_linux_x64_release_wheel" ] || fail "plain default is $plain_default"
[ "$plain_default" != "$installed_default" ] || fail "both modes default to $plain_default"

dir="$scratch/build"
record="$dir/.gweni-wheel-config"
prepare() { "$script" --build-dir "$dir" --dry-run "$@" > "$scratch/out" 2>&1 || { cat "$scratch/out"; fail "--dry-run $* failed"; }; }
expect_arg() { grep -qxF -- "$1" "$scratch/out" || { cat "$scratch/out"; fail "no configure argument $1"; }; }
expect_cleared() { grep -q "clearing it" "$scratch/out" || { cat "$scratch/out"; fail "$1: not cleared"; }; [ ! -e "$dir/marker" ] || fail "$1: the marker survived"; }
expect_kept() { ! grep -q "clearing it" "$scratch/out" || { cat "$scratch/out"; fail "$1: cleared"; }; [ -e "$dir/marker" ] || fail "$1: the marker is gone"; }

# 2. recorded for its mode
prepare
[ "$(cat "$record")" = "manifest" ] || fail "plain record: $(cat "$record")"
expect_arg "build-dir=$dir"
expect_arg "-DVCPKG_MANIFEST_MODE=ON"

# 3. another mode or vcpkg_installed clears it, the same keeps it
touch "$dir/marker"
prepare --vcpkg-installed "$installed_a"
expect_cleared "manifest to installed"
[ "$(cat "$record")" = "installed $installed_a" ] || fail "installed record: $(cat "$record")"
expect_arg "-DVCPKG_MANIFEST_MODE=OFF"
expect_arg "-DVCPKG_INSTALLED_DIR=$installed_a"

touch "$dir/marker"
prepare --vcpkg-installed "$installed_a"
expect_kept "the same vcpkg_installed again"

prepare --vcpkg-installed "$installed_b"
expect_cleared "another vcpkg_installed"

touch "$dir/marker"
prepare
expect_cleared "installed to manifest"
[ "$(cat "$record")" = "manifest" ] || fail "plain record again: $(cat "$record")"

touch "$dir/marker"
prepare
expect_kept "manifest again"

# 4. a directory from before the record is cleared; one not ours is refused
rm -rf "$dir"
mkdir -p "$dir"
touch "$dir/marker"
echo "CMAKE_HOME_DIRECTORY:INTERNAL=$repo" > "$dir/CMakeCache.txt"
prepare
expect_cleared "a configured directory with no record"

dir="$scratch/other"
mkdir -p "$dir"
touch "$dir/marker"
if "$script" --build-dir "$dir" --dry-run > "$scratch/out" 2>&1; then
    cat "$scratch/out"
    fail "a directory that is not ours was taken"
fi
grep -q "is not a wheel build directory of this checkout" "$scratch/out" || { cat "$scratch/out"; fail "no refusal line"; }
[ -e "$dir/marker" ] && [ ! -e "$dir/.gweni-wheel-config" ] || fail "the refused directory was changed"

rm -rf "$scratch"
echo "python_wheel_build_dirs: ok"
