#!/usr/bin/env python3
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The instructions each gweni_bench phase cost, from a callgrind profile of
#  gweni_bench (tools/profile.sh writes it):
#
#      tools/callgrind_phases.py callgrind.out > phases.txt
#
#  The bench runs each phase's work inside a marker function of its own,
#  gweni_bench_phase_<name> (bench/bench.h, "Profiling"), and collects only
#  while a phase runs. A phase's cost is its marker's inclusive cost: the sum
#  of the call costs callgrind recorded for every call to it, which is exact.
#  The flush marker is called from inside the render one (the renderer's
#  end()), so render is the render marker's cost minus the flush marker's.
#
#  Output, the interface tools/bench.sh reads, one line each:
#
#      phase=construct ir=<n>    the build scene's Scene::build(); 0 in the others
#      phase=input ir=<n>
#      phase=update ir=<n>
#      phase=render ir=<n>       without the flush
#      phase=flush ir=<n>
#      phase=gpu ir=<n>
#      phase=other ir=<n>        collected but in no marker: the few instructions
#                                between switching collection on and the call
#      total ir=<n>              everything collected; the lines above add up to it
#
#  Exit status 1, with a message, if the file has no Ir events or no phase.

import re
import sys

PHASES = ["construct", "input", "update", "render", "flush", "gpu"]
MARKER_PREFIX = "gweni_bench_phase_"

# fn= and cfn= share one name space for compressed names, as do the file
# specifications (fl=, fi=, fe=, cfi=, cfl=) and the objects (ob=, cob=); only
# function names matter here.
FUNCTION_KEYS = ("fn", "cfn")
NAME_RE = re.compile(r"^\((\d+)\)(?:\s(.*))?$")


def fail(message):
    sys.stderr.write("callgrind_phases.py: %s\n" % message)
    sys.exit(1)


def parse(path):
    functions = {}          # compressed id -> name
    positions = 1           # columns before the costs on a cost line
    ir_column = None        # index of Ir among the events
    callee = None           # the last cfn=
    pending_call = False    # the next cost line is a call's inclusive cost
    inclusive = {}          # marker name -> summed call costs
    total = 0
    part_total = None       # totals: (or summary:) of the current part

    def function_name(value):
        match = NAME_RE.match(value)
        if not match:
            return value
        if match.group(2) is not None:
            functions[match.group(1)] = match.group(2)
            return match.group(2)
        return functions.get(match.group(1), value)

    with open(path, "r", errors="replace") as profile:
        for line in profile:
            line = line.rstrip("\n")
            if not line:
                continue

            if pending_call and (line[0].isdigit() or line[0] in "+-*"):
                pending_call = False
                fields = line.split()
                column = positions + (ir_column or 0)
                cost = int(fields[column]) if len(fields) > column else 0
                if callee and callee.startswith(MARKER_PREFIX):
                    inclusive[callee] = inclusive.get(callee, 0) + cost
                continue

            key, sep, value = line.partition("=")
            if sep and key in FUNCTION_KEYS:
                name = function_name(value)
                if key == "cfn":
                    callee = name
                continue
            if sep and key == "calls":
                pending_call = True
                continue

            key, sep, value = line.partition(":")
            if not sep:
                continue
            value = value.strip()
            if key == "positions":
                positions = len(value.split())
            elif key == "events":
                events = value.split()
                if "Ir" not in events:
                    fail("%s: no Ir events (events: %s)" % (path, value))
                ir_column = events.index("Ir")
            elif key == "part":
                if part_total is not None:
                    total += part_total
                part_total = None
            elif key in ("summary", "totals"):
                # totals: comes after summary: and wins
                fields = value.split()
                if ir_column is not None and len(fields) > ir_column:
                    part_total = int(fields[ir_column])

    if part_total is not None:
        total += part_total
    if ir_column is None:
        fail("%s: not a callgrind profile (no events: line)" % path)
    return inclusive, total


def main():
    if len(sys.argv) != 2 or sys.argv[1] in ("-h", "--help"):
        sys.stderr.write("Usage: tools/callgrind_phases.py <callgrind.out>\n")
        sys.exit(0 if len(sys.argv) == 2 else 2)

    inclusive, total = parse(sys.argv[1])
    if not inclusive:
        fail("%s: no call to any %s* marker; is it a gweni_bench profile?"
             % (sys.argv[1], MARKER_PREFIX))

    cost = {phase: inclusive.get(MARKER_PREFIX + phase, 0) for phase in PHASES}
    cost["render"] -= cost["flush"]
    other = total - sum(cost.values())

    for phase in PHASES:
        print("phase=%s ir=%d" % (phase, cost[phase]))
    print("phase=other ir=%d" % other)
    print("total ir=%d" % total)


if __name__ == "__main__":
    main()
