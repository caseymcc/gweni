#!/usr/bin/env python3
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Writes tools/bench.sh compare's markdown report from the raw outputs it
#  collected:
#
#      tools/bench_report.py <results dir> > report.md
#
#  The results dir (bench.sh makes it):
#      meta.txt               key=value: the commits, sizes, options, durations
#      notes.txt              one note per line, shown under the summary
#      bench_diff.txt         git diff --stat of bench/ between the sides
#      scenes.txt             the scenes compared, in order
#      <side>/list.txt        gweni_bench --list of the side (base or rev)
#      <side>/counters/<scene>-<N>.txt   gweni_bench's key=value output
#      <side>/ir/<scene>-<N>/phases.txt  tools/profile.sh --phases-only
#      <side>/time/<scene>-<N>-<r>.txt   timed runs, r = 1..repeat
#
#  The flags (AGENTS.md, "Comparing two revisions"):
#      counters      any value that differs
#      instructions  a phase more than IR_FLAG_PERCENT % above the base, if
#                    that phase is at least IR_MIN_SHARE of the scene's total
#      scaling       a per-frame counter of a local scene larger at the large
#                    N than at the small N on rev, where it was not on base
#                    (a PINNED one: different at all); CHUNK_COUNTERS and
#                    SHOWN_COUNTERS (but where PINNED) grow by design
#      wall time     a phase TIME_FLAG_PERCENT % slower, slower by more than
#                    twice the larger side's MAD and by at least TIME_MIN_US

import math
import os
import re
import sys

# 1 %: above the layout noise tools/profile.sh measured between two binaries
# (under 0.4 % of update, under 0.1 % of every other phase).
IR_FLAG_PERCENT = 1.0
# A phase under 0.1 % of the scene's instructions (gpu under callgrind, other)
# is shown but not flagged: a few hundred instructions moving is not a cost.
IR_MIN_SHARE = 0.001
TIME_FLAG_PERCENT = 5.0
# A phase of a fraction of a microsecond (update on a frame with nothing to
# lay out) doubles from timer noise alone: a slowdown smaller than this is
# shown, not flagged.
TIME_MIN_US = 2.0

# Sizes, not work: they grow with N by definition.
SIZE_COUNTERS = {"prim_ids", "text_ids", "mirror_bytes", "primitive_slots_culled", "text_runs_culled"}
# One draw per index chunk of 16383 slots (and per atlas for text), so they
# grow with the slot count in every scene.
CHUNK_COUNTERS = {"draw_calls", "text_draws"}
# The primitive slots the draws cover and their ranges follow the shown slots,
# which grow with N in every scene but those PINNED for them.
SHOWN_COUNTERS = {"primitive_slots_drawn", "primitive_ranges"}
# bench/scaling.cmake's PINNED: (scene, counter) pairs whose shown part is
# fixed, so any change between the sizes, up or down, is a finding.
PINNED = {("hidden-pages", "primitive_slots_drawn"), ("hidden-pages", "primitive_ranges")}
# Of the summary's own fields, the ones that are counts of work.
SUMMARY_COUNTERS = ["controls", "construct_allocs", "construct_alloc_bytes",
                    "construct_invalidations"]
PHASES = ["construct", "input", "update", "render", "flush", "gpu"]
IR_PHASES = PHASES + ["other"]
# The counters a linear scene's growth factor is given for.
GROWTH_COUNTERS = ["visited", "z_visited", "prim_slots_written", "text_slots_written",
                   "upload_bytes", "allocs"]
NOT_COUNTERS = {"scene", "n", "frame"}


def read_lines(path):
    if not os.path.exists(path):
        return []
    with open(path, "r", errors="replace") as f:
        return [line.rstrip("\n") for line in f]


def read_meta(results):
    meta = {}
    for line in read_lines(os.path.join(results, "meta.txt")):
        key, _, value = line.partition("=")
        meta[key] = value
    return meta


def read_list(path):
    scenes = {}
    for line in read_lines(path):
        fields = line.split()
        if not fields:
            continue
        info = {"order": len(scenes)}
        for field in fields[1:]:
            key, sep, value = field.partition("=")
            if not sep:
                break
            info[key] = value
        scenes[fields[0]] = info
    return scenes


def parse_bench(path):
    """gweni_bench's key=value output: (frames, summary), or None."""
    frames = []
    summary = None
    for line in read_lines(path):
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "summary":
            summary = dict(f.partition("=")[::2] for f in fields[1:])
        elif fields[0].startswith("scene="):
            frames.append(dict(f.partition("=")[::2] for f in fields))
    if summary is None:
        return None
    return frames, summary


def number(text):
    try:
        return int(text)
    except ValueError:
        return float(text)


def median(values):
    values = sorted(values)
    if not values:
        return 0
    mid = len(values) // 2
    if len(values) % 2:
        return values[mid]
    return (values[mid - 1] + values[mid]) / 2


def mad(values):
    m = median(values)
    return median([abs(v - m) for v in values])


def counter_names(frames):
    names = []
    for frame in frames[:1]:
        for key in frame:
            if key in NOT_COUNTERS or key.endswith("_us"):
                continue
            names.append(key)
    return names


def counter_stats(parsed):
    """{(counter, stat): value} for a run: per-frame median, max and total."""
    frames, summary = parsed
    stats = {}
    for name in counter_names(frames):
        values = [number(frame[name]) for frame in frames if name in frame]
        stats[(name, "median")] = median(values)
        stats[(name, "max")] = max(values) if values else 0
        stats[(name, "total")] = sum(values)
    for name in SUMMARY_COUNTERS:
        if name in summary:
            stats[(name, "run")] = number(summary[name])
    return stats


def fmt(value):
    if isinstance(value, float):
        if value == int(value):
            return "{:,}".format(int(value))
        return "{:,.1f}".format(value)
    return "{:,}".format(value)


def fmt_delta(delta):
    text = fmt(delta)
    return "+" + text if delta > 0 else text


def fmt_ir(value):
    if value >= 10_000_000:
        return "{:,.1f} M".format(value / 1e6)
    if value >= 10_000:
        return "{:,.1f} K".format(value / 1e3)
    return "{:,}".format(value)


def percent(base, rev):
    if base == 0:
        return None if rev == 0 else math.inf
    return (rev - base) * 100.0 / base


def fmt_percent(p):
    if p is None:
        return "0 %"
    if math.isinf(p):
        return "new"
    return "{:+.2f} %".format(p)


def scene_sizes(results, side, scene):
    """The N a scene's counter outputs exist for on a side, ascending."""
    directory = os.path.join(results, side, "counters")
    sizes = []
    if os.path.isdir(directory):
        prefix = scene + "-"
        for name in os.listdir(directory):
            if name.startswith(prefix) and name.endswith(".txt"):
                size = name[len(prefix):-4]
                if size.isdigit():
                    sizes.append(int(size))
    return sorted(sizes)


class Report:
    def __init__(self, results):
        self.results = results
        self.meta = read_meta(results)
        self.lists = {side: read_list(os.path.join(results, side, "list.txt"))
                      for side in ("base", "rev")}
        self.scenes = [s for s in read_lines(os.path.join(results, "scenes.txt")) if s]
        self.out = []
        self.flags = {"counters": [], "instructions": [], "scaling": [], "time": []}

    def emit(self, line=""):
        self.out.append(line)

    def label(self, side):
        return self.meta.get(side + "_label", side)

    def both(self, scene):
        return scene in self.lists["base"] and scene in self.lists["rev"]

    def presence(self, scene):
        if self.both(scene):
            return None
        return "new" if scene in self.lists["rev"] else "removed"

    def bench(self, side, kind, scene, n):
        return parse_bench(os.path.join(self.results, side, kind, "%s-%d.txt" % (scene, n)))

    # -- Counters -------------------------------------------------------------
    def counters(self):
        body = []
        unchanged_total = 0
        changed_total = 0
        for scene in self.scenes:
            presence = self.presence(scene)
            if presence:
                body.append("- `%s`: **%s** (only on %s), nothing to compare" %
                            (scene, presence, "rev" if presence == "new" else "base"))
                continue
            sizes = sorted(set(scene_sizes(self.results, "base", scene)) &
                           set(scene_sizes(self.results, "rev", scene)))
            rows = []
            unchanged = 0
            for n in sizes:
                base = self.bench("base", "counters", scene, n)
                rev = self.bench("rev", "counters", scene, n)
                if base is None or rev is None:
                    continue
                if len(base[0]) != len(rev[0]):
                    rows.append("| %d | measured frames | | %d | %d | %+d ⚠ |" %
                                (n, len(base[0]), len(rev[0]), len(rev[0]) - len(base[0])))
                bs, rs = counter_stats(base), counter_stats(rev)
                for key in list(rs) + [k for k in bs if k not in rs]:
                    b, r = bs.get(key), rs.get(key)
                    if b == r:
                        unchanged += 1
                        continue
                    name, stat = key
                    if b is None or r is None:
                        rows.append("| %d | `%s` | %s | %s | %s | %s ⚠ |" %
                                    (n, name, stat, "—" if b is None else fmt(b),
                                     "—" if r is None else fmt(r),
                                     "new" if b is None else "removed"))
                    else:
                        rows.append("| %d | `%s` | %s | %s | %s | %s ⚠ |" %
                                    (n, name, stat, fmt(b), fmt(r), fmt_delta(r - b)))
                    self.flags["counters"].append((scene, n, name, stat, b, r))
            unchanged_total += unchanged
            changed_total += len(rows)
            sizes_text = ", ".join("N=%d" % n for n in sizes)
            if not rows:
                body.append("- `%s` (%s): all %d values unchanged" % (scene, sizes_text, unchanged))
                continue
            body.append("")
            body.append("**`%s`** (%s): %d changed, %d unchanged not shown" %
                        (scene, sizes_text, len(rows), unchanged))
            body.append("")
            body.append("| N | counter | per frame | base | rev | Δ |")
            body.append("|---:|---|---|---:|---:|---:|")
            body.extend(rows)
            body.append("")

        self.emit("## Counters")
        self.emit()
        self.emit("Exact work counters of each scene at each size, per measured frame (`median`, "
                  "`max`, `total` over the frames; `run` for the summary's own counts), base → rev. "
                  "Every value that differs is flagged ⚠: the counters are deterministic, so any "
                  "change is a change in the work done (or in the scene, if `bench/` differs). "
                  "%d values unchanged and not shown, %d changed." % (unchanged_total, changed_total))
        self.emit()
        self.out.extend(body)
        self.emit()

    # -- Instructions -----------------------------------------------------------
    def phases(self, side, scene):
        directory = os.path.join(self.results, side, "ir")
        if not os.path.isdir(directory):
            return None, None
        for name in os.listdir(directory):
            if name.startswith(scene + "-") and name[len(scene) + 1:].isdigit():
                path = os.path.join(directory, name, "phases.txt")
                values = {}
                for line in read_lines(path):
                    match = re.match(r"^(?:phase=(\w+)|(total)) ir=(\d+)$", line)
                    if match:
                        values[match.group(1) or "total"] = int(match.group(3))
                if values:
                    return int(name[len(scene) + 1:]), values
        return None, None

    def instructions(self):
        self.emit("## Instructions")
        self.emit()
        ir_n = int(self.meta.get("ir_n", "0") or 0)
        if ir_n == 0:
            self.emit("Not measured (`--no-ir`).")
            self.emit()
            return
        self.emit("Instructions of each phase under callgrind (`tools/profile.sh --phases-only` of "
                  "this checkout on each side's binary, N=%d, or the scene's default N if it takes "
                  "none), base → rev. Flagged ⚠ when a phase is more than %.0f %% above the base "
                  "and at least %.1f %% of the scene's total; %.0f %% is above the layout noise "
                  "between two binaries (under 0.4 %% of `update`, under 0.1 %% elsewhere; "
                  "AGENTS.md, \"Profiling a scene\"). Fewer instructions are shown, not flagged. "
                  "`flush` holds llvmpipe's rasterising (it rasterises inside `glFlush` with "
                  "`LP_NUM_THREADS=0`); `other` is the bench's own few instructions between "
                  "phases." % (ir_n, IR_FLAG_PERCENT, IR_MIN_SHARE * 100, IR_FLAG_PERCENT))
        self.emit()
        self.emit("| scene | N | phase | base | rev | Δ | Δ % |")
        self.emit("|---|---:|---|---:|---:|---:|---:|")
        missing = []
        for scene in self.scenes:
            presence = self.presence(scene)
            if presence:
                self.emit("| `%s` | | | | | | %s |" % (scene, presence))
                continue
            n_base, base = self.phases("base", scene)
            n_rev, rev = self.phases("rev", scene)
            if base is None or rev is None:
                missing.append(scene)
                continue
            total = max(base.get("total", 0), rev.get("total", 0))
            for phase in IR_PHASES + ["total"]:
                b, r = base.get(phase, 0), rev.get(phase, 0)
                if b == 0 and r == 0:
                    continue
                p = percent(b, r)
                flag = ""
                if (phase != "other" and r > b and p is not None and p > IR_FLAG_PERCENT
                        and max(b, r) >= IR_MIN_SHARE * total):
                    flag = " ⚠"
                    if phase != "total":
                        self.flags["instructions"].append((scene, phase, b, r, p))
                name = "**total**" if phase == "total" else phase
                self.emit("| `%s` | %d | %s | %s | %s | %s | %s%s |" %
                          (scene, n_rev, name, fmt_ir(b), fmt_ir(r), fmt_delta(r - b),
                           fmt_percent(p), flag))
        self.emit()
        if missing:
            self.emit("No instructions for %s: a side's bench predates the phase markers." %
                      ", ".join("`%s`" % s for s in missing))
            self.emit()

    # -- Scaling ---------------------------------------------------------------
    def growth(self, side, scene):
        """{counter: (small median, small max, large median, large max)} or None."""
        sizes = scene_sizes(self.results, side, scene)
        if len(sizes) < 2:
            return None, None
        small, large = sizes[0], sizes[-1]
        a = self.bench(side, "counters", scene, small)
        b = self.bench(side, "counters", scene, large)
        if a is None or b is None:
            return None, None
        sa, sb = counter_stats(a), counter_stats(b)
        result = {}
        for name in counter_names(b[0]):
            result[name] = (sa.get((name, "median"), 0), sa.get((name, "max"), 0),
                            sb.get((name, "median"), 0), sb.get((name, "max"), 0))
        return (small, large), result

    @staticmethod
    def grew(values):
        small_median, small_max, large_median, large_max = values
        return large_median > small_median or large_max > small_max

    @staticmethod
    def moved(scene, name, values):
        """Whether a counter is O(N) at the large size: grew, or for a PINNED
        one changed at all."""
        if (scene, name) in PINNED:
            small_median, small_max, large_median, large_max = values
            return large_median != small_median or large_max != small_max
        return Report.grew(values)

    @staticmethod
    def by_design(scene, name):
        """Why a counter grows with N in a local scene by design, or None."""
        if name in CHUNK_COUNTERS:
            return "chunks"
        if name in SHOWN_COUNTERS and (scene, name) not in PINNED:
            return "shown slots"
        return None

    def scaling(self):
        self.emit("## Scaling")
        self.emit()
        self.emit("A `scaling=local` scene changes a fixed part of the tree each frame, so its "
                  "per-frame counters must be the same at the large N as at the small one; a "
                  "counter whose per-frame median or max grew is O(N). Flagged ⚠ when it grows on "
                  "rev and did not on base: a **new O(N)**. One that grows on both is listed as "
                  "known, unflagged. `%s` are sizes and left out; `%s` grow with the slot count by "
                  "design (one draw per index chunk of 16383 slots), and `%s` with the shown "
                  "slots (the primitive slots the draws cover and their ranges), never flagged "
                  "but in %s, where any change is (bench_scaling's PINNED)." %
                  ("`, `".join(sorted(SIZE_COUNTERS)), "`, `".join(sorted(CHUNK_COUNTERS)),
                   "`, `".join(sorted(SHOWN_COUNTERS)),
                   ", ".join("`%s` `%s`" % pair for pair in sorted(PINNED))))
        self.emit()
        new_rows, known_rows, fixed_rows, clean = [], [], [], []
        linear = []
        for scene in self.scenes:
            info = self.lists["rev"].get(scene) or self.lists["base"].get(scene) or {}
            scaling = info.get("scaling", "")
            sizes, rev = self.growth("rev", scene)
            if rev is None:
                continue
            _, base = self.growth("base", scene) if scene in self.lists["base"] else (None, None)
            if scaling != "local":
                linear.append((scene, sizes, rev))
                continue
            any_row = False
            for name, values in rev.items():
                if name in SIZE_COUNTERS:
                    continue
                grows = self.moved(scene, name, values)
                base_grows = base is not None and name in base and self.moved(scene, name, base[name])
                design = self.by_design(scene, name)
                row = "| `%s` | `%s` | %s / %s | %s / %s |" % (
                    scene, name, fmt(values[0]), fmt(values[1]), fmt(values[2]), fmt(values[3]))
                if grows and base is not None and not base_grows and design is None:
                    new_rows.append(row + " **new O(N)** ⚠ |")
                    self.flags["scaling"].append((scene, name, values))
                    any_row = True
                elif grows:
                    note = design or (
                        "known (grows on base too)" if base is not None
                        else "grows (base has no such scene, so nothing to compare with)")
                    known_rows.append(row + " %s |" % note)
                    any_row = True
                elif base_grows and design is None:
                    fixed_rows.append(row + " fixed (grew on base) |")
                    any_row = True
            if not any_row:
                clean.append(scene)
        header = ["| scene | counter | N=%s median / max | N=%s median / max | |" %
                  (self.meta.get("n_small"), self.meta.get("n_large")),
                  "|---|---|---:|---:|---|"]
        if new_rows or fixed_rows or known_rows:
            self.out.extend(header)
            self.out.extend(new_rows + fixed_rows + known_rows)
            self.emit()
        if clean:
            self.emit("Local scenes with every work counter the same at both sizes: %s." %
                      ", ".join("`%s`" % s for s in clean))
            self.emit()
        if linear:
            self.emit("Linear scenes, O(N) by design: the large N's per-frame median over the "
                      "small N's (on rev).")
            self.emit()
            self.emit("| scene | " + " | ".join("`%s`" % c for c in GROWTH_COUNTERS) + " |")
            self.emit("|---|" + "---:|" * len(GROWTH_COUNTERS))
            for scene, sizes, rev in linear:
                cells = []
                for name in GROWTH_COUNTERS:
                    if name not in rev:
                        cells.append("—")
                        continue
                    small, large = rev[name][0], rev[name][2]
                    if small == 0:
                        cells.append("0 → 0" if large == 0 else "0 → %s" % fmt(large))
                    else:
                        cells.append("×%.1f" % (large / small))
                self.emit("| `%s` | %s |" % (scene, " | ".join(cells)))
            self.emit()

    # -- Wall time -------------------------------------------------------------
    def timed_runs(self, side, scene):
        directory = os.path.join(self.results, side, "time")
        runs = []
        if not os.path.isdir(directory):
            return runs
        pattern = re.compile(r"^%s-\d+-\d+\.txt$" % re.escape(scene))
        for name in sorted(os.listdir(directory)):
            if pattern.match(name):
                parsed = parse_bench(os.path.join(directory, name))
                if parsed:
                    runs.append(parsed[1])
        return runs

    def wall_time(self):
        if self.meta.get("timed") != "1":
            return
        self.emit("## Wall time")
        self.emit()
        self.emit("%s timed runs of each scene per side at N=%s, in rounds (every scene once per side per round, base and rev taking turns going first), each "
                  "holding the render and quiet locks (no other job holding a lock while it ran). Per run, a phase's time is its median over the "
                  "measured frames (µs; `construct` is the build itself); the table gives the "
                  "median of the runs ± their MAD (median absolute deviation). Flagged ⚠ when rev "
                  "is more than %.0f %% slower, slower by more than twice the larger MAD and by "
                  "at least %.0f µs (below that a phase is timer noise). "
                  "`gpu` is llvmpipe rasterising on the CPU, not a GPU." %
                  (self.meta.get("repeat"), self.meta.get("time_n"), TIME_FLAG_PERCENT, TIME_MIN_US))
        self.emit()
        if self.meta.get("locked") != "1":
            self.emit("**These runs were not locked (no `mm`): other jobs may have run alongside, "
                      "and the times are not trustworthy.**")
            self.emit()
        self.emit("| scene | phase | base | rev | Δ % |")
        self.emit("|---|---|---:|---:|---:|")
        for scene in self.scenes:
            if not self.both(scene):
                continue
            base, rev = self.timed_runs("base", scene), self.timed_runs("rev", scene)
            if not base or not rev:
                continue
            for phase in PHASES:
                key = "construct_us" if phase == "construct" else phase + "_us_median"
                bv = [number(run[key]) for run in base if key in run]
                rv = [number(run[key]) for run in rev if key in run]
                if not bv or not rv:
                    continue
                bm, rm = median(bv), median(rv)
                if bm == 0 and rm == 0:
                    continue
                spread = max(mad(bv), mad(rv))
                p = percent(bm, rm)
                flag = ""
                if p is not None and not math.isinf(p) and p > TIME_FLAG_PERCENT and rm - bm > 2 * spread \
                        and rm - bm >= TIME_MIN_US:
                    flag = " ⚠"
                    self.flags["time"].append((scene, phase, bm, rm, p))
                name = phase + (" (llvmpipe, not a GPU)" if phase == "gpu" else "")
                self.emit("| `%s` | %s | %.1f ± %.1f | %.1f ± %.1f | %s%s |" %
                          (scene, name, bm, mad(bv), rm, mad(rv), fmt_percent(p), flag))
        self.emit()

    # -- The whole report ----------------------------------------------------------
    def summary(self):
        meta = self.meta
        lines = ["# gweni_bench: %s → %s" % (self.label("base"), self.label("rev")), ""]
        lines.append("- **base** `%s` %s" % (meta.get("base_sha", "")[:10], meta.get("base_subject", "")))
        lines.append("- **rev** %s: `%s` %s" % (self.label("rev"), meta.get("rev_sha", "")[:10],
                                                meta.get("rev_subject", "")))
        lines.append("- sizes N=%s and N=%s; instructions: %s; %s" % (
            meta.get("n_small"), meta.get("n_large"),
            "N=%s" % meta.get("ir_n") if meta.get("ir_n", "0") != "0" else "not measured",
            "wall time: %s runs at N=%s" % (meta.get("repeat"), meta.get("time_n"))
            if meta.get("timed") == "1" else "no wall time (`--time` not given)"))
        lines.append("- took %ss to build, %ss for the counters, %ss under callgrind, %ss timed" % (
            meta.get("build_seconds"), meta.get("counters_seconds"), meta.get("ir_seconds"),
            meta.get("time_seconds")))
        diff = [l for l in read_lines(os.path.join(self.results, "bench_diff.txt")) if l.strip()]
        if diff:
            lines.append("- **`bench/` differs between the two sides**, so changed scenes move "
                         "their numbers by design:")
            lines.append("")
            lines.append("  ```")
            lines.extend("  " + l for l in diff)
            lines.append("  ```")
        else:
            lines.append("- `bench/` is the same on both sides")
        lines.append("")

        f = self.flags
        parts = []
        parts.append("%d counter value%s changed%s" % (
            len(f["counters"]), "" if len(f["counters"]) == 1 else "s",
            " (in %s)" % ", ".join("`%s`" % s for s in sorted({c[0] for c in f["counters"]}))
            if f["counters"] else ""))
        if meta.get("ir_n", "0") != "0":
            worst = sorted(f["instructions"], key=lambda x: -x[4])[:3]
            parts.append("%d instruction flag%s%s" % (
                len(f["instructions"]), "" if len(f["instructions"]) == 1 else "s",
                " (worst: %s)" % ", ".join("`%s` %s %s" % (s, ph, fmt_percent(p))
                                           for s, ph, _, _, p in worst) if worst else ""))
        parts.append("%d new O(N) counter%s in local scenes%s" % (
            len(f["scaling"]), "" if len(f["scaling"]) == 1 else "s",
            " (%s)" % ", ".join("`%s` %s" % (s, c) for s, c, _ in f["scaling"][:5])
            if f["scaling"] else ""))
        if meta.get("timed") == "1":
            worst = sorted(f["time"], key=lambda x: -x[4])[:3]
            parts.append("%d wall-time flag%s%s" % (
                len(f["time"]), "" if len(f["time"]) == 1 else "s",
                " (worst: %s)" % ", ".join("`%s` %s %s" % (s, ph, fmt_percent(p))
                                           for s, ph, _, _, p in worst) if worst else ""))
        total = sum(len(v) for v in f.values())
        verdict = ("**No flags**: the work counters%s are unchanged within the thresholds." %
                   (" and instructions" if meta.get("ir_n", "0") != "0" else "")
                   if total == 0 else "**%d flag%s**, to accept or fix." % (total, "" if total == 1 else "s"))
        if not any(self.both(scene) for scene in self.scenes):
            verdict = "**Nothing to compare**: no scene is on both sides, so nothing is flagged."
        lines.append("%s %s." % (verdict, "; ".join(parts)[0].upper() + "; ".join(parts)[1:]))
        lines.append("")

        presence = [(s, self.presence(s)) for s in self.scenes if self.presence(s)]
        if presence:
            lines.append("Scenes on one side only: %s." %
                         ", ".join("`%s` %s" % (s, p) for s, p in presence))
            lines.append("")
        notes = []
        for note in read_lines(os.path.join(self.results, "notes.txt")):
            if note and note not in notes:
                notes.append(note)
        for note in notes:
            lines.append("> %s" % note)
        if notes:
            lines.append("")
        return lines

    def write(self):
        self.counters()
        self.instructions()
        self.scaling()
        self.wall_time()
        return "\n".join(self.summary() + self.out) + "\n"


def main():
    if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]):
        sys.stderr.write("usage: bench_report.py <results dir>\n")
        sys.exit(2)
    sys.stdout.write(Report(sys.argv[1]).write())


if __name__ == "__main__":
    main()
