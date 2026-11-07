#!/usr/bin/env bash
#
#  Profiles one gweni_bench scene with callgrind: where the instructions of
#  its measured frames go, per phase, per function and per source line.
#
#  Inside the build container, after building the profile tree:
#
#      ./generate.sh linux profile
#      cmake --build build_linux_x64_profile -j
#      tools/profile.sh hover-sweep --n 1000
#      tools/profile.sh build --n 10000 --out /tmp/build10k
#
#  Usage: tools/profile.sh <scene> [--n N] [--frames F] [--out DIR]
#                          [--bin-dir DIR] [--graph-thres P] [--graph-root FN]
#                          [--layout-pad K] [-- <more gweni_bench args>]
#
#    <scene>        a scene of gweni_bench --list
#    --n N          the scene's size (default: the scene's default N)
#    --frames F     measured frames (default: the scene's)
#    --out DIR      where the outputs go; default /tmp/gweni-profile-<scene>-<N>
#                   (relative to the cwd). Existing outputs there are replaced.
#    --bin-dir DIR  where gweni_bench lives (relative to the cwd); default
#                   build_linux_x64_profile/bench in the repo
#    --graph-thres P  callgraph.svg leaves out functions under P % of the
#                   profile and calls under P/5 % (default 0.5). In a local scene
#                   llvmpipe is nearly all of it, so gweni's functions show only
#                   with a small P, e.g. 0.001
#    --graph-root FN  callgraph.svg shows only FN and what it calls, e.g.
#                   gweni_bench_phase_update (gprof2dot -z; the percentages stay
#                   shares of the whole profile)
#    --layout-pad K  run the bench from a directory K characters longer
#                   (/tmp/gweni-profile-run-xxx...), which moves its heap: the
#                   counts of runs at several K show the layout noise (below)
#    -- ARGS        passed to gweni_bench as they are (--k, --warmup, --size, ...);
#                   it runs in /tmp/gweni-profile-run, so a --resources here must
#                   be absolute
#    --help         this text
#
#  It writes into DIR:
#    callgrind.out  the profile, for kcachegrind/qcachegrind (on a host:
#                   `qcachegrind DIR/callgrind.out`) or callgrind_annotate
#    phases.txt     the instructions of each phase (tools/callgrind_phases.py
#                   describes the lines); the interface tools/bench.sh reads
#    annotate.txt   the phases, the gweni sources the profile touched, the
#                   functions by inclusive cost and every line of those sources
#                   with its cost (callgrind_annotate --inclusive=yes: a line's
#                   own instructions plus those of the calls it makes)
#    callgraph.svg  gprof2dot's call graph, nodes and edges coloured by their
#                   inclusive share of the profile, hot paths red
#    bench.txt      gweni_bench's own output (its counters and times; the
#                   times are callgrind's, about 50x slower than a real run)
#    valgrind.log   valgrind's messages
#
#  Only the measured frames are collected (and the build scene's construction):
#  gweni_bench switches instrumentation and collection with client requests
#  (bench/bench.h, "Profiling"), so EGL and GL init, loading the skin and
#  fonts, the shader primer, the warm-up frames and teardown are not in the
#  profile. llvmpipe runs with LP_NUM_THREADS=0, so it rasterises on the main
#  thread, inside the flush phase's glFlush(). The bench runs as a copy at a
#  fixed path, from a fixed directory, in a fixed environment (below), with
#  the shader cache off and its own fixed getrandom() under valgrind
#  (bench/fixedRandom.cpp): the same binary, scene and arguments give the same
#  counts in every run, to the instruction, whatever the cwd, the environment
#  or where the binary was built. Two different binaries also lay the heap out
#  differently, and glibc's malloc does more or less work for it: that layout
#  noise, measured with --layout-pad, is under 0.4 % of update and under
#  0.1 % of every other phase (AGENTS.md, "Profiling a scene"). Mesa's own
#  functions are the anonymous ???:0x... in swrast_dri.so (no symbols), its
#  JIT-compiled shaders ???:0x... in no object.
#
#  A run holds the GPU lock (mm lock acquire --until-exit) when mm is on PATH,
#  and runs unlocked with a note otherwise. Exit status: 0 when every output
#  was written, 1 if the bench is missing or the run or a tool failed, 2 for a
#  bad argument.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR=build_linux_x64_profile
BIN_DIR="$BUILD_DIR/bench"

usage() {
    sed -n '/^#  Usage:/,/^#    --help/p' "${BASH_SOURCE[0]}" | sed 's/^#//; s/^  //'
}

die() {
    echo "profile.sh: $*" >&2
    exit 1
}

scene=""
n=""
frames=""
outDir=""
graphThres=0.5
graphRoot=""
layoutPad=0
binDirGiven=0
benchArgs=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --n|--frames|--out|--bin-dir|--graph-thres|--graph-root|--layout-pad)
            if [[ $# -lt 2 ]]; then
                echo "profile.sh: $1 needs a value" >&2
                exit 2
            fi
            case "$1" in
                --n) n="$2" ;;
                --frames) frames="$2" ;;
                --out) outDir="$2" ;;
                --bin-dir) BIN_DIR="$2"; binDirGiven=1 ;;
                --graph-thres) graphThres="$2" ;;
                --graph-root) graphRoot="$2" ;;
                --layout-pad) layoutPad="$2" ;;
            esac
            shift
            ;;
        --)
            shift
            benchArgs=("$@")
            break
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "profile.sh: unexpected option $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            if [[ -n "$scene" ]]; then
                echo "profile.sh: one scene only ($scene, then $1)" >&2
                exit 2
            fi
            scene="$1"
            ;;
    esac
    shift
done

if [[ -z "$scene" ]]; then
    echo "profile.sh: no scene given" >&2
    usage >&2
    exit 2
fi

if ! [[ "$layoutPad" =~ ^[0-9]+$ ]]; then
    echo "profile.sh: --layout-pad needs a count of characters, not $layoutPad" >&2
    exit 2
fi

# A relative --bin-dir means the caller's cwd; the default means the repo root.
if [[ $binDirGiven -eq 0 ]]; then
    BIN_DIR="$ROOT/$BIN_DIR"
fi
BENCH="$BIN_DIR/gweni_bench"

if [[ ! -x "$BENCH" ]]; then
    echo "profile.sh: $BENCH is missing; build it first:" >&2
    if [[ $binDirGiven -eq 0 ]]; then
        echo "    ./generate.sh linux profile" >&2
        echo "    cmake --build $BUILD_DIR -j" >&2
    else
        echo "    cmake --build <the build tree holding $BIN_DIR> -j" >&2
    fi
    exit 1
fi

for tool in valgrind callgrind_annotate gprof2dot dot python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        die "$tool is not installed; the build image has it (Dockerfile), so this container predates it"
    fi
done

# The scene's default N, and whether it exists at all.
listLine=$("$BENCH" --list | awk -v s="$scene" '$1 == s')
if [[ -z "$listLine" ]]; then
    echo "profile.sh: no such scene \"$scene\"; the scenes are:" >&2
    "$BENCH" --list | awk '{print "  " $1}' >&2
    exit 2
fi
if [[ -z "$n" ]]; then
    n=$(sed -n 's/.* default_n=\([0-9]*\).*/\1/p' <<<"$listLine")
fi

if [[ -z "$outDir" ]]; then
    outDir="/tmp/gweni-profile-$scene-$n"
fi
mkdir -p "$outDir"
outDir="$(cd "$outDir" && pwd)"

profile="$outDir/callgrind.out"
phases="$outDir/phases.txt"
annotate="$outDir/annotate.txt"
callgraph="$outDir/callgraph.svg"
benchOut="$outDir/bench.txt"
valgrindLog="$outDir/valgrind.log"
rm -f "$profile" "$phases" "$annotate" "$callgraph" "$benchOut" "$valgrindLog"

args=(--scene "$scene" --n "$n")
if [[ -n "$frames" ]]; then
    args+=(--frames "$frames")
fi
args+=("${benchArgs[@]}")

# The GPU lock, held until the run is over (or this script exits).
lease=""
if command -v mm >/dev/null 2>&1; then
    lockReply=$(mm lock acquire --until-exit $$ --label "profile.sh $scene" gpu) \
        || die "could not take the gpu lock"
    lease=$(python3 -I -c 'import json, sys; print(json.load(sys.stdin).get("lease") or "")' \
        <<<"$lockReply")
else
    echo "profile.sh: mm is not on PATH, so this runs without the gpu lock"
fi

# The run's surroundings are fixed, so its counts depend only on the binary,
# its resources and the arguments: a copy of gweni_bench at a fixed path (its
# argv[0] and /proc/self/exe), run from that directory with the resources
# linked into it (--resources resources, so every path the bench builds has
# the same length), in a fixed environment. The lengths of argv, the
# environment and those paths move the stack and the heap, and glibc's malloc
# and memcmp then do a different amount of work (build --n 10000: update
# 1879.5 M to 1932.5 M over six resource-path lengths). Runs on one host take
# turns on the directory (flock).
stage=/tmp/gweni-profile-run
if [[ $layoutPad -gt 0 ]]; then
    stage="$stage-$(printf '%*s' "$layoutPad" '' | tr ' ' x)"
fi
mkdir -p "$stage" || die "cannot create $stage"
exec 9>/tmp/gweni-profile-run.lock
flock 9 || die "cannot lock /tmp/gweni-profile-run.lock"
rm -rf "${stage:?}/gweni_bench" "$stage/resources"
cp "$BENCH" "$stage/gweni_bench" || die "cannot copy $BENCH to $stage"
if [[ ! -d "$BIN_DIR/resources" ]]; then
    die "$BIN_DIR/resources is missing; cmake --build copies it next to gweni_bench"
fi
ln -s "$(cd "$BIN_DIR/resources" && pwd)" "$stage/resources"

echo "profiling: gweni_bench ${args[*]}"
start=$SECONDS
status=0
# LP_NUM_THREADS=0: llvmpipe rasterises inline, deterministically.
# MESA_SHADER_CACHE_DISABLE: a shader variant first met in a measured frame is
# compiled in every run, not loaded from the cache in some.
# HOME: Mesa looks for ~/.drirc there; the stage has none.
# GLIBC_TUNABLES: how much work glibc's malloc does depends on the heap's
# layout (consolidating fastbins, searching its bins), so a change that only
# moves the heap moved update by up to 6 %. With no fastbins (mxfast=0), every
# small free kept in the thread cache (tcache_count) and a fixed mmap threshold
# it moves update by under 0.4 % and the other phases by under 0.05 % (AGENTS.md,
# "Profiling a scene"). glibc's share of the counts is then that of this
# configuration, not of the default one.
(cd "$stage" && env -i PATH=/usr/bin:/bin HOME="$stage" \
    GLIBC_TUNABLES=glibc.malloc.mxfast=0:glibc.malloc.tcache_count=65535:glibc.malloc.mmap_threshold=131072 \
    LP_NUM_THREADS=0 MESA_SHADER_CACHE_DISABLE=true \
    "$(command -v valgrind)" --tool=callgrind --instr-atstart=no --collect-atstart=no \
        --dump-instr=yes --collect-jumps=yes \
        --callgrind-out-file="$profile" --log-file="$valgrindLog" \
        "$stage/gweni_bench" --resources resources "${args[@]}") >"$benchOut" 2>&1 || status=$?
elapsed=$((SECONDS-start))

exec 9>&-
if [[ -n "$lease" ]]; then
    mm lock release "$lease" >/dev/null || true
fi

if [[ $status -ne 0 || ! -s "$profile" ]]; then
    echo "profile.sh: gweni_bench failed under callgrind (exit $status):" >&2
    cat "$benchOut" "$valgrindLog" >&2
    exit 1
fi

python3 -I "$ROOT/tools/callgrind_phases.py" "$profile" >"$phases" \
    || die "could not read the phases from $profile (a gweni_bench built without <valgrind/callgrind.h> collects nothing)"

# gweni's own sources the profile has costs in: source/, include/ and bench/,
# not the system's, vcpkg's or deps/.
mapfile -t sources < <(grep -oE "^(fl|fi|fe|cfi|cfl)=\([0-9]+\) $ROOT/(source|include|bench)/[^ ]+" "$profile" \
    | sed 's/^[a-z]*=([0-9]*) //' | sort -u)

{
    echo "gweni_bench ${args[*]}"
    echo "Profiled: the measured frames and the build scene's construction, nothing else"
    echo "(not setup, EGL/GL init, resource loading, the shader primer, warm-up or teardown)."
    echo "Under callgrind llvmpipe rasterises in the flush phase (glFlush); ???:0x... is Mesa"
    echo "(swrast_dri.so has no symbols) or its JIT-compiled shaders."
    echo "A recursive function's second level is name'2; its inclusive cost counts the recursion"
    echo "more than once, so it can show more than 100 %."
    echo
    echo "Phases (phases.txt):"
    sed 's/^/  /' "$phases"
    echo
    echo "gweni sources annotated below (${#sources[@]}):"
    for source in "${sources[@]}"; do
        echo "  ${source#"$ROOT"/}"
    done
    echo
    callgrind_annotate --inclusive=yes --threshold=99.9 --auto=no "$profile" "${sources[@]}"
} >"$annotate" 2>"$outDir/annotate.log" || die "callgrind_annotate failed: $(cat "$outDir/annotate.log")"
rm -f "$outDir/annotate.log"

# By default a node is a function with at least 0.5 % of the profile and an
# edge a call with 0.1 %: gprof2dot's defaults, enough to follow every phase
# down to where its cost goes without the graph turning into a wall.
edgeThres=$(python3 -I -c 'import sys; print(float(sys.argv[1])/5)' "$graphThres") \
    || die "--graph-thres $graphThres is not a number"
graphArgs=(--node-thres="$graphThres" --edge-thres="$edgeThres")
if [[ -n "$graphRoot" ]]; then
    graphArgs+=(--root="$graphRoot")
fi
graphDot="$outDir/callgraph.dot"
gprof2dot -f callgrind --strip --wrap "${graphArgs[@]}" -o "$graphDot" "$profile" \
    2>"$outDir/gprof2dot.log" || die "gprof2dot failed: $(cat "$outDir/gprof2dot.log")"
# gprof2dot exits 0 when --root names no function, and draws nothing
if [[ -s "$outDir/gprof2dot.log" ]]; then
    die "gprof2dot: $(cat "$outDir/gprof2dot.log")"
fi
dot -Tsvg -o "$callgraph" "$graphDot" || die "dot failed"
rm -f "$graphDot" "$outDir/gprof2dot.log"

echo "callgrind ran ${elapsed}s"
sed 's/^/  /' "$phases"
for out in "$profile" "$phases" "$annotate" "$callgraph" "$benchOut"; do
    echo "  $out"
done
