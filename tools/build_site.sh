#!/usr/bin/env bash
#
#  Builds the release web tree and stages the hosted site: one static directory
#  that GitHub Pages (or any static server) serves as it is.
#
#  Inside the build container (it needs emsdk and vcpkg, as ./generate.sh web
#  does), from any directory:
#
#      ./tools/build_site.sh                       # stage site/ in the repo
#      ./tools/build_site.sh --out /tmp/site       # stage it elsewhere
#      ./tools/build_site.sh --build-dir DIR       # take the pages from DIR
#
#  Options:
#    --out DIR        where to stage the site (default site/ in the repo, which
#                     is git-ignored). A directory that is not empty must hold a
#                     site this script staged (its .gweni-site marker): its old
#                     contents are removed first, and anything else is refused.
#    --build-dir DIR  the web tree to build and take the pages from (default
#                     build_web_wasm32_release, configured with
#                     ./generate.sh web release if it is not yet). Another tree
#                     must be configured already.
#    --help           this text.
#
#  It builds ControlTest and OpenGLCore in that tree (cmake --build) and stages:
#    index.html         the landing page, with docs/images/demo.png as a preview
#    demo.html          the OpenGLCore demo's interactive window
#    controltest.html   ControlTest's window with a control picker, whose
#                       entries are "ControlTest --list" (run under node);
#                       ?control=<Name>&scale=<s> is ControlTest's argv
#    site.css           their one stylesheet (tools/site/, as the pages are)
#    ControlTest.{js,wasm,data}, OpenGLCore.{js,wasm,data}
#                       the two apps, each .data the preloaded resources/
#    images/demo.png    the landing page's preview
#    licenses/          gweni's LICENSE and the fonts' licenses
#    .nojekyll          so Pages serves the files as they are
#  Every URL in it is relative, so it works under https://<user>.github.io/gweni/
#  and from a local server alike. It needs no COOP/COEP headers: the build has
#  no threads and no SharedArrayBuffer.
#
#  Then it prints what each page sends over the network, every file raw, gzip
#  and brotli (tools/site/sizes.mjs, nothing taken to be cached), and exits 1 if
#  the gallery page's gzip total is over BUDGET_BYTES. web_site_smoke
#  (tests/web/site.cmake) runs it and loads every page in headless Chromium.
#
#  It takes no lock: building takes none, and node --list opens no context.
#  The build runs at most CMAKE_BUILD_PARALLEL_LEVEL jobs, nproc unless the
#  environment sets it.

set -euo pipefail

: "${CMAKE_BUILD_PARALLEL_LEVEL:=$(nproc)}"
export CMAKE_BUILD_PARALLEL_LEVEL

# What the gallery page (controltest.html and everything it loads) may send, gzip.
BUDGET_BYTES=8000000
BUDGET_PAGE=controltest.html

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$repo/site"
build_dir="$repo/build_web_wasm32_release"
default_build_dir=1

usage()
{
    sed -n '2,/^$/{s/^#  \{0,1\}//;p}' "${BASH_SOURCE[0]}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out)
            [ $# -ge 2 ] || { echo "build_site.sh: --out needs a directory" >&2; exit 1; }
            out="$(realpath -m "$2")"; shift 2 ;;
        --build-dir)
            [ $# -ge 2 ] || { echo "build_site.sh: --build-dir needs a directory" >&2; exit 1; }
            build_dir="$(realpath -m "$2")"; default_build_dir=; shift 2 ;;
        --help|-h)
            usage; exit 0 ;;
        *)
            echo "build_site.sh: unknown argument: $1" >&2
            usage >&2
            exit 1 ;;
    esac
done

node="${EMSDK_NODE:-node}"
if ! command -v "$node" >/dev/null; then
    echo "build_site.sh: no node (EMSDK_NODE, or node on PATH): run it inside the build container" >&2
    exit 1
fi

# configure the release web tree if it is not yet, then build the two apps
if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    if [ -z "$default_build_dir" ]; then
        echo "build_site.sh: $build_dir is not a configured tree: ./generate.sh web [release] configures one" >&2
        exit 1
    fi
    (cd "$repo" && ./generate.sh web release)
fi
cmake --build "$build_dir" --target ControlTest OpenGLCore

bin="$build_dir/examples/openglcore"
for file in ControlTest.js ControlTest.wasm ControlTest.data OpenGLCore.js OpenGLCore.wasm OpenGLCore.data; do
    if [ ! -f "$bin/$file" ]; then
        echo "build_site.sh: $bin/$file is missing: is $build_dir a web tree (./generate.sh web release)?" >&2
        exit 1
    fi
done

# the control picker's entries: "  <Name>[ (<what it is>)]" lines after the heading
controls="$(cd "$bin" && "$node" ControlTest.js --list)"

# the staging dir: empty, or a site staged here before
if [ -e "$out" ] && [ ! -d "$out" ]; then
    echo "build_site.sh: $out is not a directory" >&2
    exit 1
fi
if [ -d "$out" ] && [ -n "$(ls -A "$out")" ] && [ ! -f "$out/.gweni-site" ]; then
    echo "build_site.sh: $out is not empty and holds no site staged by this script (.gweni-site): not touching it" >&2
    exit 1
fi
mkdir -p "$out"
find "$out" -mindepth 1 -delete
touch "$out/.gweni-site" "$out/.nojekyll"

site="$repo/tools/site"
cp "$site/index.html" "$site/demo.html" "$site/site.css" "$out/"
cp "$bin"/ControlTest.{js,wasm,data} "$bin"/OpenGLCore.{js,wasm,data} "$out/"
mkdir -p "$out/images" "$out/licenses"
cp "$repo/docs/images/demo.png" "$out/images/"
cp "$repo/LICENSE" "$out/licenses/LICENSE.txt"
cp "$repo"/resources/*-LICENSE.txt "$out/licenses/"

# controltest.html with one <option> per control where its gweni:controls comment is
GWENI_CONTROLS="$controls" python3 -I - "$site/controltest.html" "$out/controltest.html" <<'EOF'
import html, os, sys
options = []
for line in os.environ['GWENI_CONTROLS'].splitlines():
    if not line.startswith('  ') or not line.strip():
        continue
    name = line.split()[0]
    options.append('<option value="%s">%s</option>' % (html.escape(name), html.escape(line.strip())))
if not options:
    sys.exit('build_site.sh: ControlTest --list listed no controls')
page = open(sys.argv[1]).read()
marker = '<!-- gweni:controls -->'
if page.count(marker) != 1:
    sys.exit('build_site.sh: %s has no single %s' % (sys.argv[1], marker))
open(sys.argv[2], 'w').write(page.replace(marker, '\n'.join(options)))
EOF

echo "build_site.sh: staged $out ($(echo "$controls" | grep -c '^  ') controls in the picker)"
"$node" "$site/sizes.mjs" "$out" --budget "$BUDGET_BYTES" --budget-page "$BUDGET_PAGE"
