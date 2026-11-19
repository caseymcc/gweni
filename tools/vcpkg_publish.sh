#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  tools/vcpkg_publish.sh [--repo DIR] [--archive FILE] [--work DIR] <tag>
#
#  Writes the vcpkg registry's commit for gweni <tag> (v<major>.<minor>.<patch>):
#  the orphan branch vcpkg-registry, which holds only ports/gweni/ and
#  versions/ (docs/building.md -> "gweni from vcpkg"). It pushes nothing, and
#  changes no branch, tag or file of the repository: it works in a scratch git
#  repository and ends by printing the commands that put the branch on GitHub
#  and the baseline consumers write in their vcpkg-configuration.json.
#
#  1. Refuses (exit 1) unless the tag exists in the repository (--repo, default
#     the checkout this script is in) and the tag, project(gweni VERSION ...)
#     in the tag's CMakeLists.txt and version-semver in its
#     ports/gweni/vcpkg.json are one version.
#  2. The SHA512 is that of GitHub's archive of the tag
#     (https://github.com/caseymcc/gweni/archive/refs/tags/<tag>.tar.gz, so
#     the tag must be pushed first), or of --archive FILE (a local one, e.g.
#     git archive --format=tar.gz --prefix=gweni-<version>/ <tag>, the tests').
#  3. A scratch repository (--work DIR, which must not exist, or a new
#     /tmp/gweni-vcpkg-registry-XXXXXX) on vcpkg-registry as the repository has
#     it (its branch vcpkg-registry, else origin/vcpkg-registry), or a new
#     orphan branch on the first run. The tag's ports/gweni/ replaces the one
#     there, its portfile's REF the tag and its SHA512 the archive's; one
#     commit, then vcpkg x-add-version writes versions/baseline.json and
#     versions/g-/gweni.json (the port's git-tree) and a second commit takes
#     them: vcpkg's two steps.
#  4. Prints the registry directory, the fetch (in the container, where the
#     scratch repository is) and push (on the host, with its credentials)
#     commands the owner runs, the baseline (the second commit's sha) and the
#     vcpkg-configuration.json consumers write, with a default registry at the
#     vcpkg commit this ran with (vcpkg wants a baseline for it too).
#
#  Needs git, vcpkg (VCPKG_ROOT's or on PATH), sha512sum and, without
#  --archive, curl. Runs in the build container, no lock.
#

set -euo pipefail

usage() {
    echo "usage: $0 [--repo DIR] [--archive FILE] [--work DIR] <tag>" >&2
    exit 2
}
fail() {
    echo "vcpkg_publish: $*" >&2
    exit 1
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/.." && pwd)"
archive=""
work=""
tag=""
while [ $# -gt 0 ]; do
    case "$1" in
        --repo) [ $# -ge 2 ] || usage; repo="$2"; shift 2 ;;
        --archive) [ $# -ge 2 ] || usage; archive="$2"; shift 2 ;;
        --work) [ $# -ge 2 ] || usage; work="$2"; shift 2 ;;
        -h|--help) usage ;;
        -*) echo "vcpkg_publish: unknown option $1" >&2; usage ;;
        *) [ -z "$tag" ] || usage; tag="$1"; shift ;;
    esac
done
[ -n "$tag" ] || usage
branch=vcpkg-registry
github_repo=caseymcc/gweni

# 1. the versions agree
[[ "$tag" =~ ^v([0-9]+\.[0-9]+\.[0-9]+)$ ]] || fail "the tag must be v<major>.<minor>.<patch>, not $tag"
version="${BASH_REMATCH[1]}"
git -C "$repo" rev-parse --verify --quiet "refs/tags/$tag^{commit}" >/dev/null \
    || fail "$repo has no tag $tag"
project_version="$(git -C "$repo" show "$tag:CMakeLists.txt" \
    | sed -n 's/^project(gweni VERSION \([0-9.]*\).*/\1/p' | head -n 1)"
[ -n "$project_version" ] || fail "$tag's CMakeLists.txt has no project(gweni VERSION ...)"
port_json="$(git -C "$repo" show "$tag:ports/gweni/vcpkg.json" 2>/dev/null)" \
    || fail "$tag has no ports/gweni/vcpkg.json"
port_version="$(printf '%s\n' "$port_json" \
    | sed -n 's/^ *"version-semver": *"\([^"]*\)".*/\1/p' | head -n 1)"
[ -n "$port_version" ] || fail "$tag's ports/gweni/vcpkg.json has no version-semver"
if [ "$version" != "$project_version" ] || [ "$version" != "$port_version" ]; then
    fail "the versions differ: tag $tag, project(gweni VERSION $project_version) in CMakeLists.txt," \
        "version-semver $port_version in ports/gweni/vcpkg.json; they must be one version"
fi

if [ -n "${VCPKG_ROOT:-}" ] && [ -x "$VCPKG_ROOT/vcpkg" ]; then
    vcpkg="$VCPKG_ROOT/vcpkg"
else
    vcpkg="$(command -v vcpkg)" || fail "no vcpkg: set VCPKG_ROOT or put vcpkg on PATH"
fi

# 2. the archive's SHA512
scratch="$(mktemp -d /tmp/gweni-vcpkg-publish-XXXXXX)"
trap 'rm -rf "$scratch"' EXIT
if [ -z "$archive" ]; then
    url="https://github.com/$github_repo/archive/refs/tags/$tag.tar.gz"
    archive="$scratch/$tag.tar.gz"
    curl -fsSL -o "$archive" "$url" \
        || fail "could not download $url (has $tag been pushed?)"
fi
[ -f "$archive" ] || fail "no archive $archive"
sha512="$(sha512sum "$archive" | cut -d' ' -f1)"

# 3. the registry's scratch repository
if [ -z "$work" ]; then
    work="$(mktemp -d /tmp/gweni-vcpkg-registry-XXXXXX)"
else
    [ ! -e "$work" ] || fail "$work exists already"
    mkdir -p "$work"
fi
git init --quiet "$work"
git_reg() { git -C "$work" "$@"; }
# who commits: the repository's user, else a name of the script's own
for key in user.name user.email; do
    value="$(git -C "$repo" config "$key" || true)"
    [ -n "$value" ] || { [ "$key" = user.name ] && value="gweni vcpkg_publish" || value="vcpkg_publish@localhost"; }
    git_reg config "$key" "$value"
done
base=""
for ref in "refs/heads/$branch" "refs/remotes/origin/$branch"; do
    if git -C "$repo" rev-parse --verify --quiet "$ref" >/dev/null; then
        base="$ref"
        break
    fi
done
if [ -n "$base" ]; then
    git_reg fetch --quiet --no-tags "$repo" "$base:refs/heads/$branch"
    git_reg checkout --quiet "$branch"
    echo "vcpkg_publish: on $base ($(git_reg rev-parse --short HEAD))"
else
    git_reg checkout --quiet --orphan "$branch"
    echo "vcpkg_publish: no $branch in $repo: a new orphan branch"
fi

git_reg rm -r --quiet --ignore-unmatch ports/gweni
rm -rf "$work/ports/gweni"
mkdir -p "$work/ports"
git -C "$repo" archive "$tag" ports/gweni | tar -x -C "$work"
portfile="$work/ports/gweni/portfile.cmake"
grep -q '^    REF "v${VERSION}"$' "$portfile" || fail "$tag's portfile has no REF \"v\${VERSION}\" line"
grep -q '^    SHA512 [0-9a-f]*$' "$portfile" || fail "$tag's portfile has no SHA512 line"
sed -i -e "s|^    REF \"v\${VERSION}\"\$|    REF \"$tag\"|" \
       -e "s|^    SHA512 [0-9a-f]*\$|    SHA512 $sha512|" "$portfile"
git_reg add ports/gweni
git_reg commit --quiet -m "gweni $version: ports/gweni from $tag"

mkdir -p "$work/versions"
(cd "$work" && "$vcpkg" x-add-version gweni \
    "--x-builtin-ports-root=$work/ports" \
    "--x-builtin-registry-versions-dir=$work/versions") \
    || fail "vcpkg x-add-version failed in $work"
git_reg add versions
git_reg commit --quiet -m "gweni $version: versions"
baseline="$(git_reg rev-parse HEAD)"

# 4. what the owner runs
origin_url="$(git -C "$repo" remote get-url origin 2>/dev/null || echo "https://github.com/$github_repo.git")"
vcpkg_baseline="$(git -C "$(dirname "$vcpkg")" rev-parse HEAD 2>/dev/null || echo "<a microsoft/vcpkg commit>")"
cat <<EOF
vcpkg_publish: gweni $version written to $branch in $work (nothing pushed)
registry: $work
baseline: $baseline
To publish it:
  1. here, where $work is (the build container: from a host,
     prefix it with ./run_docker.sh, whose container keeps its /tmp), take the
     branch into the repository:
    git -C "$repo" fetch "$work" $branch:$branch
  2. on the host, in that checkout, with its GitHub credentials (the container
     has none), push it ($origin_url):
    git push origin $branch
Consumers then write, in vcpkg-configuration.json (the default registry needs
a baseline too: any microsoft/vcpkg commit, here the one this ran with):
    {"default-registry": {"kind": "builtin", "baseline": "$vcpkg_baseline"},
     "registries": [{"kind": "git", "repository": "https://github.com/$github_repo",
       "reference": "$branch", "baseline": "$baseline", "packages": ["gweni"]}]}
EOF
