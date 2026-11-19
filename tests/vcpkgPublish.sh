#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  vcpkg_publish: tools/vcpkg_publish.sh writes a vcpkg registry vcpkg resolves
#  gweni from, and refuses a tag whose versions differ. Run by ctest
#  (tests/CMakeLists.txt, every native tree, label package, no lock):
#
#      tests/vcpkgPublish.sh <repo> <scratch dir> <vcpkg root>
#
#  It works on a scratch git repository holding this checkout's CMakeLists.txt
#  and ports/gweni/ as they are on disk (uncommitted changes included), with
#  local tags, and an archive of each tag made by git archive (--archive), so
#  nothing is downloaded or pushed. It fails, saying which, unless
#  1. the script refuses, writing no registry, a tag that is not
#     project()'s and the port's version (v0.1.5 on 0.1.0), a port version that
#     is not project()'s (v0.2.5, the port 0.2.5, project() 0.1.0) and a
#     project() version that is not the port's (v0.3.0, project() 0.3.0, the
#     port 0.1.0);
#  2. v0.1.0 gives a new orphan branch vcpkg-registry of two commits holding
#     only ports/gweni/ and versions/: the portfile's REF the tag and its
#     SHA512 the archive's, versions/g-/gweni.json 0.1.0 at the git-tree of
#     ports/gweni, versions/baseline.json 0.1.0, the printed baseline its
#     head;
#  3. vcpkg install --dry-run of a manifest depending on gweni, with that
#     registry (a file path as repository) at the printed baseline, plans
#     gweni 0.1.0 from it at that git-tree, building nothing;
#  4. the printed fetch command takes the branch into the repository, and a
#     second version (v0.4.0, both bumped) goes on top of it: two versions in
#     gweni.json, the newest first, baseline.json 0.4.0, the first baseline
#     its ancestor, still only ports/gweni/ and versions/.
#

set -uo pipefail

if [ $# -ne 3 ]; then
    echo "usage: $0 <repo> <scratch dir> <vcpkg root>" >&2
    exit 2
fi
source_dir="$1"
scratch="$2"
export VCPKG_ROOT="$3"
publish="$source_dir/tools/vcpkg_publish.sh"

failures=0
fail() {
    echo "vcpkg_publish: FAIL: $*" >&2
    failures=$((failures + 1))
}
die() {
    echo "vcpkg_publish: FAIL: $*" >&2
    exit 1
}

# a stale run must not be able to pass
rm -rf "$scratch"
mkdir -p "$scratch"
repo="$scratch/repo"
mkdir -p "$repo/ports"
git init --quiet "$repo"
git_repo() { git -C "$repo" -c user.name=vcpkg_publish -c user.email=vcpkg_publish@localhost "$@"; }
cp "$source_dir/CMakeLists.txt" "$repo/"
cp -r "$source_dir/ports/gweni" "$repo/ports/"
git_repo add -A
git_repo commit --quiet -m "gweni" || die "could not commit the scratch repository"
base="$(git_repo rev-parse HEAD)"
project_version="$(sed -n 's/^project(gweni VERSION \([0-9.]*\).*/\1/p' "$repo/CMakeLists.txt")"
[ "$project_version" = "0.1.0" ] || die "project(gweni VERSION ...) is '$project_version', this test is written for 0.1.0"

# a commit on top of the base with the project's and the port's versions set
# ("" keeps one), tagged
set_versions() {
    local project="$1" port="$2" tag="$3"
    git_repo checkout --quiet --detach "$base"
    if [ -n "$project" ]; then
        sed -i "s/^project(gweni VERSION [0-9.]*/project(gweni VERSION $project/" "$repo/CMakeLists.txt"
    fi
    if [ -n "$port" ]; then
        sed -i "s/\"version-semver\": \"[^\"]*\"/\"version-semver\": \"$port\"/" "$repo/ports/gweni/vcpkg.json"
    fi
    git_repo commit --quiet -am "versions $project $port"
    git_repo tag "$tag"
}
archive_of() {
    git_repo archive --format=tar.gz --prefix="gweni-${1#v}/" "$1" > "$scratch/$1.tar.gz"
    echo "$scratch/$1.tar.gz"
}

# 1. the refusals
git_repo tag v0.1.5 "$base"
set_versions "" 0.2.5 v0.2.5
set_versions 0.3.0 "" v0.3.0
git_repo checkout --quiet --detach "$base"
for tag in v0.1.5 v0.2.5 v0.3.0; do
    output="$("$publish" --repo "$repo" --archive "$(archive_of "$tag")" --work "$scratch/refused-$tag" "$tag" 2>&1)"
    status=$?
    if [ $status -ne 1 ] || ! grep -q "the versions differ" <<<"$output"; then
        fail "$tag was not refused (exit $status): $output"
    fi
    if [ -e "$scratch/refused-$tag" ]; then
        fail "$tag was refused but wrote $scratch/refused-$tag"
    fi
done

# 2. the first version, a new orphan branch
git_repo tag v0.1.0 "$base"
archive="$(archive_of v0.1.0)"
registry="$scratch/registry"
output="$("$publish" --repo "$repo" --archive "$archive" --work "$registry" v0.1.0 2>&1)"
status=$?
echo "$output"
[ $status -eq 0 ] || die "v0.1.0 failed (exit $status)"
baseline="$(sed -n 's/^baseline: //p' <<<"$output")"
[ -n "$baseline" ] || die "v0.1.0 printed no baseline"
git_reg() { git -C "$registry" "$@"; }

check_registry() {
    local what="$1" head="$2" stray
    stray="$(git_reg ls-tree -r --name-only "$head" | grep -v -e '^ports/gweni/' -e '^versions/')"
    [ -z "$stray" ] || fail "$what: the branch holds more than ports/gweni/ and versions/: $stray"
    [ "$(git_reg symbolic-ref --short HEAD)" = vcpkg-registry ] || fail "$what: the registry is not on vcpkg-registry"
}
check_registry v0.1.0 "$baseline"
[ "$(git_reg rev-parse HEAD)" = "$baseline" ] || fail "the printed baseline $baseline is not the branch's head"
[ "$(git_reg rev-list --count HEAD)" = 2 ] || fail "v0.1.0's branch is not two commits: $(git_reg log --oneline)"
[ "$(git_reg rev-list --max-parents=0 HEAD)" = "$(git_reg rev-parse HEAD~1)" ] || fail "v0.1.0's first commit has a parent"
grep -qx '    REF "v0.1.0"' "$registry/ports/gweni/portfile.cmake" \
    || fail "the portfile's REF is not the tag: $(grep REF "$registry/ports/gweni/portfile.cmake")"
sha512="$(sha512sum "$archive" | cut -d' ' -f1)"
grep -qx "    SHA512 $sha512" "$registry/ports/gweni/portfile.cmake" \
    || fail "the portfile's SHA512 is not the archive's ($sha512): $(grep SHA512 "$registry/ports/gweni/portfile.cmake")"
tree="$(git_reg rev-parse "HEAD:ports/gweni")"
versions_json="$(python3 -c 'import json,sys; v=json.load(open(sys.argv[1]))["versions"]; print(" ".join(e["version-semver"]+"@"+e["git-tree"] for e in v))' "$registry/versions/g-/gweni.json")"
[ "$versions_json" = "0.1.0@$tree" ] || fail "versions/g-/gweni.json is '$versions_json', not 0.1.0@$tree"
baseline_json="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["default"]["gweni"]["baseline"])' "$registry/versions/baseline.json")"
[ "$baseline_json" = 0.1.0 ] || fail "versions/baseline.json has gweni $baseline_json, not 0.1.0"

# 3. vcpkg resolves gweni 0.1.0 from it, building nothing
manifest="$scratch/manifest"
mkdir -p "$manifest" "$scratch/registries-cache"
builtin_baseline="$(git -C "$VCPKG_ROOT" rev-parse HEAD)"
cat > "$manifest/vcpkg.json" <<EOF
{"name": "registry-consumer", "version": "0", "builtin-baseline": "$builtin_baseline",
 "dependencies": [{"name": "gweni", "version>=": "0.1.0"}]}
EOF
cat > "$manifest/vcpkg-configuration.json" <<EOF
{"registries": [{"kind": "git", "repository": "$registry", "reference": "vcpkg-registry",
  "baseline": "$baseline", "packages": ["gweni"]}]}
EOF
plan="$(cd "$scratch" && X_VCPKG_REGISTRIES_CACHE="$scratch/registries-cache" "$VCPKG_ROOT/vcpkg" install --dry-run \
    "--x-manifest-root=$manifest" "--x-install-root=$scratch/installed" \
    "--x-buildtrees-root=$scratch/buildtrees" "--x-packages-root=$scratch/packages" \
    --triplet x64-linux 2>&1)"
status=$?
echo "$plan" | grep gweni
if [ $status -ne 0 ] || ! grep -qF "gweni[core,opengl]:x64-linux@0.1.0 -- git+$registry@$tree" <<<"$plan"; then
    fail "vcpkg did not plan gweni 0.1.0 from the registry at $tree (exit $status): $plan"
fi
[ -e "$scratch/installed/x64-linux" ] && fail "the dry run installed something"

# 4. the branch fetched as printed, and a second version on top of it
fetch_command="$(sed -n 's/^    \(git -C .* fetch .*\)$/\1/p' <<<"$output")"
[ -n "$fetch_command" ] || die "v0.1.0 printed no fetch command"
eval "$fetch_command" || die "the printed fetch command failed: $fetch_command"
set_versions 0.4.0 0.4.0 v0.4.0
registry="$scratch/registry2"
output="$("$publish" --repo "$repo" --archive "$(archive_of v0.4.0)" --work "$registry" v0.4.0 2>&1)"
status=$?
echo "$output"
[ $status -eq 0 ] || die "v0.4.0 failed (exit $status)"
second="$(sed -n 's/^baseline: //p' <<<"$output")"
check_registry v0.4.0 "$second"
git_reg merge-base --is-ancestor "$baseline" "$second" || fail "v0.4.0's branch does not go on from v0.1.0's $baseline"
tree2="$(git_reg rev-parse "HEAD:ports/gweni")"
versions_json="$(python3 -c 'import json,sys; v=json.load(open(sys.argv[1]))["versions"]; print(" ".join(e["version-semver"]+"@"+e["git-tree"] for e in v))' "$registry/versions/g-/gweni.json")"
[ "$versions_json" = "0.4.0@$tree2 0.1.0@$tree" ] || fail "after v0.4.0, versions/g-/gweni.json is '$versions_json', not 0.4.0@$tree2 0.1.0@$tree"
baseline_json="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["default"]["gweni"]["baseline"])' "$registry/versions/baseline.json")"
[ "$baseline_json" = 0.4.0 ] || fail "after v0.4.0, versions/baseline.json has gweni $baseline_json, not 0.4.0"

if [ $failures -ne 0 ]; then
    echo "vcpkg_publish: $failures check(s) failed" >&2
    exit 1
fi
echo "vcpkg_publish: passed"
