#!/usr/bin/env bash
#
#  Compares what gweni_bench's scenes cost on two revisions: the work
#  counters, the instructions (callgrind) and, with --time, the wall time, per
#  scene and phase, as one markdown report. It flags what got worse; it is not
#  a gate, and exits 0 whatever the report says.
#
#  Inside the build container (from any cwd):
#
#      tools/bench.sh compare origin/main            # origin/main vs the working tree
#      tools/bench.sh compare origin/main --keep     # keep origin/main's build for next time
#      tools/bench.sh compare HEAD~3 HEAD --scenes hover-sweep,type
#      tools/bench.sh compare origin/main --time     # wall time too (holds the quiet lock)
#
#  Usage: tools/bench.sh compare <base-rev> [<rev>] [options]
#
#    <base-rev>     the baseline: any commit git rev-parse takes
#    <rev>          the revision to weigh; default the working tree, with its
#                   uncommitted changes
#    --scenes A,B   only these scenes (default: every scene either side has)
#    --n-small N    the small size each scene runs at (default 1000)
#    --n-large N    the large size, for the scaling check (default 10000)
#    --ir-n N       the size the instructions are measured at (default the
#                   small N); 0, or --no-ir, measures none
#    --no-ir        no instructions (no callgrind; much faster)
#    --time         measure wall time too: R rounds, each running every scene
#                   once per side (A B, then B A in the next round), each run
#                   holding the render and quiet locks
#    --repeat R     timed runs per scene and side (default 5)
#    --time-n N     the size the times are measured at (default the small N)
#    --out FILE     the report (default /tmp/gweni-bench-<base>-<rev>.md); the
#                   raw outputs go into FILE without .md, plus .d
#    --keep         keep the exported sources and builds of the commits
#                   compared, so the next compare with them builds nothing
#    --help         this text
#
#  The sides. A commit is exported (git archive, not a git worktree: nothing is
#  registered in the repository) to /tmp/gweni-bench-src-<sha>/ and built
#  there in its profile tree (./generate.sh linux profile, then cmake --build
#  --target gweni_bench). An export that is already there (a --keep of an
#  earlier compare) is reused as it is and left in place; one made by this run
#  is removed at the end unless --keep. The working tree is built in its own
#  build_linux_x64_profile/, in place, so its uncommitted changes are in it
#  and an existing profile build of it is reused incrementally. A revision
#  with no bench/ (from before gweni_bench) is not built, and its scenes are
#  reported as new; one from before the profile tree is built as its release
#  tree, and the report says so.
#
#  The runs. Every gweni_bench run is a copy of that side's binary at one
#  fixed path, /tmp/gweni-bench-run/gweni_bench, run from that directory with
#  its own resources linked in, in a fixed environment (env -i with PATH,
#  HOME, LIBGL_ALWAYS_SOFTWARE=1 and MESA_SHADER_CACHE_DISABLE=true), so where
#  a side was built does not change what it does. A counter run sets
#  LP_NUM_THREADS=0, as callgrind's do; a timed run leaves llvmpipe its
#  threads. The instructions are
#  tools/profile.sh --phases-only of this checkout, run on each side's binary,
#  so both sides are measured by the same harness. Each run takes its locks
#  on its own, never around a batch: a counter run mm run --lock render (it
#  opens a GL context, on llvmpipe), a timed one --lock render --lock quiet
#  (quiet waits until no other job holds a lock and keeps them off while the
#  run lasts, so the times are of a quiet host). The callgrind runs take no
#  lock: instruction counts do not depend on the host's load. Without mm on
#  PATH everything runs unlocked, and the report says so.
#
#  The report (tools/bench_report.py writes it from the raw outputs): the
#  counters, the instructions, the scaling check and the wall times, each
#  with its flags, and a summary paragraph at the top. AGENTS.md, "Comparing
#  two revisions", describes every section and flag.
#
#  Exit status: 0 when everything built and ran, whatever the report says; 1
#  when a build, a run or a tool failed (the step is named on stderr); 2 for a
#  bad argument.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SELF="$ROOT/tools/bench.sh"
STAGE=/tmp/gweni-bench-run
STAGE_LOCK=/tmp/gweni-bench-run.lock

usage() {
    sed -n '/^#  Usage:/,/^#    --help/p' "${BASH_SOURCE[0]}" | sed 's/^#//; s/^  //'
}

die() {
    echo "bench.sh: $*" >&2
    exit 1
}

bad_arg() {
    echo "bench.sh: $*" >&2
    exit 2
}

#-----------------------------------------------------------------------------
# __run: one gweni_bench run, staged (what mm run calls, holding the locks)
#
#   bench.sh __run <bench dir> <output file> <counters|time> <gweni_bench args...>
#-----------------------------------------------------------------------------
run_staged() {
    local binDir="$1" out="$2" mode="$3"
    shift 3

    mkdir -p "$STAGE" || die "cannot create $STAGE"
    exec 9>"$STAGE_LOCK"
    flock 9 || die "cannot lock $STAGE_LOCK"
    rm -rf "${STAGE:?}/gweni_bench" "$STAGE/resources"
    cp "$binDir/gweni_bench" "$STAGE/gweni_bench" || die "cannot copy $binDir/gweni_bench to $STAGE"
    [[ -d "$binDir/resources" ]] || die "$binDir/resources is missing; cmake --build copies it next to gweni_bench"
    ln -s "$(cd "$binDir/resources" && pwd)" "$STAGE/resources"

    # LIBGL_ALWAYS_SOFTWARE: llvmpipe even where the host's GPU is passed in,
    # as the image sets it (env -i would drop it), so both sides draw with the
    # same rasteriser as every other run here.
    local envArgs=(PATH=/usr/bin:/bin HOME="$STAGE" LIBGL_ALWAYS_SOFTWARE=1 MESA_SHADER_CACHE_DISABLE=true)
    if [[ "$mode" == counters ]]; then
        envArgs+=(LP_NUM_THREADS=0)
    fi

    local status=0
    (cd "$STAGE" && env -i "${envArgs[@]}" "$STAGE/gweni_bench" --resources resources "$@") \
        >"$out" 2>"$out.err" || status=$?
    exec 9>&-
    return $status
}

if [[ "${1:-}" == __run ]]; then
    shift
    run_staged "$@"
    exit $?
fi

#-----------------------------------------------------------------------------
# Arguments
#-----------------------------------------------------------------------------
if [[ $# -eq 0 ]]; then
    usage >&2
    exit 2
fi

case "$1" in
    -h|--help) usage; exit 0 ;;
    compare) shift ;;
    *) echo "bench.sh: unknown command $1" >&2; usage >&2; exit 2 ;;
esac

baseRev=""
rev=""
scenesArg=""
nSmall=1000
nLarge=10000
irN=""
noIr=0
timed=0
repeat=5
timeN=""
out=""
keep=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenes|--n-small|--n-large|--ir-n|--repeat|--time-n|--out)
            [[ $# -ge 2 ]] || bad_arg "$1 needs a value"
            case "$1" in
                --scenes) scenesArg="$2" ;;
                --n-small) nSmall="$2" ;;
                --n-large) nLarge="$2" ;;
                --ir-n) irN="$2" ;;
                --repeat) repeat="$2" ;;
                --time-n) timeN="$2" ;;
                --out) out="$2" ;;
            esac
            shift
            ;;
        --no-ir) noIr=1 ;;
        --time) timed=1 ;;
        --keep) keep=1 ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "bench.sh: unexpected option $1" >&2; usage >&2; exit 2 ;;
        *)
            if [[ -z "$baseRev" ]]; then
                baseRev="$1"
            elif [[ -z "$rev" ]]; then
                rev="$1"
            else
                bad_arg "compare takes two revisions at most ($baseRev, $rev, then $1)"
            fi
            ;;
    esac
    shift
done

[[ -n "$baseRev" ]] || bad_arg "compare needs a base revision"
[[ -n "$irN" ]] || irN="$nSmall"
[[ -n "$timeN" ]] || timeN="$nSmall"
if [[ $noIr -eq 1 ]]; then
    irN=0
fi
for value in "$nSmall" "$nLarge" "$irN" "$repeat" "$timeN"; do
    [[ "$value" =~ ^[0-9]+$ ]] || bad_arg "a size or count must be a whole number, not $value"
done
[[ "$nSmall" -gt 0 && "$nLarge" -gt 0 && "$repeat" -gt 0 && "$timeN" -gt 0 ]] \
    || bad_arg "the sizes and --repeat must be at least 1"

for tool in git python3 cmake flock; do
    command -v "$tool" >/dev/null 2>&1 || die "$tool is not installed"
done
if [[ $irN -gt 0 ]]; then
    command -v valgrind >/dev/null 2>&1 \
        || die "valgrind is not installed (the build image has it, so this container predates it); --no-ir compares without instructions"
fi
[[ -n "${VCPKG_ROOT:-}" ]] || die "VCPKG_ROOT is not set: run this inside the build container (./run_docker.sh tools/bench.sh ...)"

haveMm=0
if command -v mm >/dev/null 2>&1; then
    haveMm=1
fi

cd "$ROOT"

baseSha=$(git rev-parse --verify --quiet "$baseRev^{commit}") || bad_arg "no such revision: $baseRev"
baseShort=$(git rev-parse --short=10 "$baseSha")
if [[ -n "$rev" ]]; then
    revSha=$(git rev-parse --verify --quiet "$rev^{commit}") || bad_arg "no such revision: $rev"
    revShort=$(git rev-parse --short=10 "$revSha")
    revLabel="$revShort"
    revFile="$revShort"
else
    revSha=""
    headShort=$(git rev-parse --short=10 HEAD)
    if [[ -n "$(git status --porcelain --untracked-files=normal -- . ':!build_*' 2>/dev/null)" ]]; then
        revLabel="working tree ($headShort + uncommitted changes)"
        revFile="$headShort-dirty"
    else
        revLabel="working tree ($headShort)"
        revFile="$headShort"
    fi
fi

[[ -n "$out" ]] || out="/tmp/gweni-bench-$baseShort-$revFile.md"
mkdir -p "$(dirname "$out")"
out="$(cd "$(dirname "$out")" && pwd)/$(basename "$out")"
results="${out%.md}.d"
rm -rf "$results"
mkdir -p "$results/base" "$results/rev"

notes="$results/notes.txt"
: >"$notes"
note() {
    echo "$*" >>"$notes"
}

if [[ $haveMm -eq 0 ]]; then
    echo "bench.sh: mm is not on PATH, so every run is unlocked"
    note "mm was not on PATH: every run ran without the render (and quiet) lock, so other jobs may have shared the host; the wall times are not trustworthy."
fi

#-----------------------------------------------------------------------------
# The sides: export and build
#-----------------------------------------------------------------------------
createdExports=()
cleanup() {
    if [[ $keep -eq 0 ]]; then
        local dir
        for dir in "${createdExports[@]}"; do
            rm -rf "$dir"
        done
    fi
}
trap cleanup EXIT

# export_commit <sha>: sets exportDir to the source dir holding <sha>'s files.
export_commit() {
    local sha="$1"
    local dir="/tmp/gweni-bench-src-$sha"
    exportDir="$dir"
    if [[ -f "$dir/.gweni-bench-export" ]]; then
        echo "bench.sh: reusing $dir"
    else
        rm -rf "$dir"
        mkdir -p "$dir"
        createdExports+=("$dir")
        git archive "$sha" | tar -x -C "$dir" || die "export of $sha (git archive) failed"
        echo "$sha" >"$dir/.gweni-bench-export"
    fi
}

# build_side <side> <label> <source dir>: builds gweni_bench there and writes
# <side>/bin_dir (empty when the side has no bench).
build_side() {
    local side="$1" label="$2" src="$3"
    local log="$results/$side/build.log"
    : >"$log"

    if [[ ! -f "$src/bench/CMakeLists.txt" ]]; then
        echo "bench.sh: $label has no bench/; its scenes count as absent"
        note "$side ($label) has no bench/ (it predates gweni_bench): nothing was run there."
        : >"$results/$side/bin_dir"
        return
    fi

    local type=profile
    if ! grep -q '"profile"' "$src/generate.sh"; then
        type=release
        note "$side ($label) predates the profile tree, so it was built as its release tree (-O3 or -O2 without -g/frame pointers): its instructions and times are of another build type than the other side's if that has one."
    fi
    local buildDir="$src/build_linux_x64_$type"

    echo "bench.sh: building $label in $buildDir (log: $log)"
    if [[ ! -f "$buildDir/CMakeCache.txt" ]]; then
        (cd "$src" && ./generate.sh linux "$type") >>"$log" 2>&1 \
            || die "configure of $side ($label) failed: ./generate.sh linux $type in $src; see $log"
    fi
    cmake --build "$buildDir" -j --target gweni_bench >>"$log" 2>&1 \
        || die "build of $side ($label) failed: cmake --build $buildDir --target gweni_bench; see $log"
    [[ -x "$buildDir/bench/gweni_bench" ]] || die "build of $side ($label) made no $buildDir/bench/gweni_bench; see $log"
    echo "$buildDir/bench" >"$results/$side/bin_dir"
    # The phase markers callgrind's per-phase counts come from (tools/profile.sh)
    if [[ -f "$src/bench/callgrind.h" ]]; then
        echo 1 >"$results/$side/has_markers"
    fi
}

buildStart=$SECONDS
export_commit "$baseSha"
baseSrc="$exportDir"
build_side base "$baseShort" "$baseSrc"
if [[ -n "$revSha" ]]; then
    export_commit "$revSha"
    revSrc="$exportDir"
else
    revSrc="$ROOT"
fi
build_side rev "$revLabel" "$revSrc"
buildSeconds=$((SECONDS-buildStart))

baseBin=$(cat "$results/base/bin_dir")
revBin=$(cat "$results/rev/bin_dir")

# Whether bench/ itself differs: a changed scene moves its numbers by design.
if [[ -n "$revSha" ]]; then
    git diff --stat "$baseSha" "$revSha" -- bench/ >"$results/bench_diff.txt"
else
    {
        git diff --stat "$baseSha" -- bench/
        git ls-files --others --exclude-standard -- bench/ | sed 's/^/ new file: /'
    } >"$results/bench_diff.txt"
fi

#-----------------------------------------------------------------------------
# The scenes
#-----------------------------------------------------------------------------
for side in base rev; do
    bin=$(cat "$results/$side/bin_dir")
    if [[ -n "$bin" ]]; then
        "$bin/gweni_bench" --list >"$results/$side/list.txt" 2>"$results/$side/list.err" \
            || die "gweni_bench --list failed on $side; see $results/$side/list.err"
    else
        : >"$results/$side/list.txt"
    fi
done

mapfile -t allScenes < <(awk '!seen[$1]++ {print $1}' "$results/rev/list.txt" "$results/base/list.txt")
if [[ -n "$scenesArg" ]]; then
    IFS=, read -r -a wanted <<<"$scenesArg"
    for scene in "${wanted[@]}"; do
        printf '%s\n' "${allScenes[@]}" | grep -qx -- "$scene" \
            || bad_arg "no side has a scene \"$scene\"; the scenes are: ${allScenes[*]}"
    done
    scenes=("${wanted[@]}")
else
    scenes=("${allScenes[@]}")
fi
printf '%s\n' "${scenes[@]}" >"$results/scenes.txt"

# list_field <side> <scene> <key>: a field of the scene's --list line.
list_field() {
    awk -v s="$2" -v k="$3" '$1 == s { for(i=2; i<=NF; ++i) if(index($i, k"=") == 1) { print substr($i, length(k)+2); exit } }' \
        "$results/$1/list.txt"
}

has_scene() {
    awk -v s="$2" '$1 == s { found=1 } END { exit found ? 0 : 1 }' "$results/$1/list.txt"
}

# locked <locks...> -- <command...>: the command holding the locks (with mm).
locked() {
    local locks=()
    while [[ "$1" != -- ]]; do
        locks+=(--lock "$1")
        shift
    done
    shift
    if [[ $haveMm -eq 1 ]]; then
        mm run "${locks[@]}" -- "$@"
    else
        "$@"
    fi
}

# bench_run <side> <mode> <file> <args...>
bench_run() {
    local side="$1" mode="$2" file="$3"
    shift 3
    local bin
    bin=$(cat "$results/$side/bin_dir")
    # render: a GL context on llvmpipe. quiet, for a timed run only: no other
    # job holds a lock while it runs. Each run takes them on its own: a hold
    # of quiet over 10 min would take it over.
    local lockList=(render)
    if [[ "$mode" == time ]]; then
        lockList+=(quiet)
    fi
    if ! locked "${lockList[@]}" -- "$SELF" __run "$bin" "$file" "$mode" "$@"; then
        echo "bench.sh: gweni_bench $* failed on $side ($bin):" >&2
        cat "$file" "$file.err" >&2 2>/dev/null || true
        die "run of scene on $side failed: gweni_bench $*"
    fi
    if ! grep -q '^summary ' "$file"; then
        die "gweni_bench $* on $side printed no summary line; see $file"
    fi
}

# The sizes a scene runs at for its counters: both, or its default if it
# takes no N.
scene_sizes() {
    local side="$1" scene="$2"
    if [[ "$(list_field "$side" "$scene" takes_n)" == 1 ]]; then
        echo "$nSmall $nLarge"
    else
        list_field "$side" "$scene" default_n
    fi
}

runStart=$SECONDS
for scene in "${scenes[@]}"; do
    for side in base rev; do
        has_scene "$side" "$scene" || continue
        mkdir -p "$results/$side/counters"
        for n in $(scene_sizes "$side" "$scene"); do
            echo "bench.sh: counters  $side $scene --n $n"
            bench_run "$side" counters "$results/$side/counters/$scene-$n.txt" --scene "$scene" --n "$n"
        done
    done
done
countersSeconds=$((SECONDS-runStart))

irSeconds=0
if [[ $irN -gt 0 ]]; then
    irStart=$SECONDS
    for scene in "${scenes[@]}"; do
        for side in base rev; do
            has_scene "$side" "$scene" || continue
            bin=$(cat "$results/$side/bin_dir")
            # A bench from before the phase markers (bench/callgrind.h) collects nothing.
            if [[ ! -f "$results/$side/has_markers" ]]; then
                note "$side's gweni_bench has no phase markers (it predates tools/profile.sh): no instructions for it."
                continue
            fi
            n="$irN"
            if [[ "$(list_field "$side" "$scene" takes_n)" != 1 ]]; then
                n=$(list_field "$side" "$scene" default_n)
            fi
            dir="$results/$side/ir/$scene-$n"
            mkdir -p "$results/$side/ir"
            echo "bench.sh: callgrind $side $scene --n $n"
            "$ROOT/tools/profile.sh" "$scene" --n "$n" --bin-dir "$bin" --out "$dir" --phases-only \
                >"$results/$side/ir/$scene-$n.log" 2>&1 \
                || { cat "$results/$side/ir/$scene-$n.log" >&2; die "callgrind run of $scene on $side failed (tools/profile.sh)"; }
        done
    done
    irSeconds=$((SECONDS-irStart))
fi

timeSeconds=0
if [[ $timed -eq 1 ]]; then
    timeStart=$SECONDS
    mkdir -p "$results/base/time" "$results/rev/time"
    # Round r runs every scene once per side before round r+1, so a scene's
    # runs are spread over the whole timed part and a burst of load from
    # another job lands on one of them, not on all of one side's. Within a
    # round the sides take turns going first (A B, then B A), so whatever a
    # run leaves behind (caches, clocks, llvmpipe's threads winding down)
    # lands on both alike.
    for ((r=1; r<=repeat; ++r)); do
        order=(base rev)
        if [[ $((r % 2)) -eq 0 ]]; then
            order=(rev base)
        fi
        for scene in "${scenes[@]}"; do
            has_scene base "$scene" && has_scene rev "$scene" || continue
            n="$timeN"
            if [[ "$(list_field rev "$scene" takes_n)" != 1 ]]; then
                n=$(list_field rev "$scene" default_n)
            fi
            for side in "${order[@]}"; do
                echo "bench.sh: time      $side $scene --n $n ($r/$repeat)"
                bench_run "$side" time "$results/$side/time/$scene-$n-$r.txt" --scene "$scene" --n "$n"
            done
        done
    done
    timeSeconds=$((SECONDS-timeStart))
fi

#-----------------------------------------------------------------------------
# The report
#-----------------------------------------------------------------------------
{
    echo "base_label=$baseShort"
    echo "base_sha=$baseSha"
    echo "base_subject=$(git log -1 --format=%s "$baseSha")"
    echo "rev_label=$revLabel"
    if [[ -n "$revSha" ]]; then
        echo "rev_sha=$revSha"
        echo "rev_subject=$(git log -1 --format=%s "$revSha")"
    else
        echo "rev_sha=$(git rev-parse HEAD)"
        echo "rev_subject=$(git log -1 --format=%s HEAD)"
    fi
    echo "n_small=$nSmall"
    echo "n_large=$nLarge"
    echo "ir_n=$irN"
    echo "timed=$timed"
    echo "repeat=$repeat"
    echo "time_n=$timeN"
    echo "locked=$haveMm"
    echo "build_seconds=$buildSeconds"
    echo "counters_seconds=$countersSeconds"
    echo "ir_seconds=$irSeconds"
    echo "time_seconds=$timeSeconds"
} >"$results/meta.txt"

python3 -I "$ROOT/tools/bench_report.py" "$results" >"$out" \
    || die "writing the report failed (tools/bench_report.py $results)"
cat "$out"
echo
echo "bench.sh: report $out, raw outputs $results"
