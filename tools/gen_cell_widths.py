#!/usr/bin/env python3
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Generates source/text/cellWidthTable.inc, the range tables of
#  gweni::text::cellCount (include/gweni/text/cells.h): how many terminal cells
#  a codepoint takes in a fixed-width run, as a terminal's wcwidth counts them.
#
#    2  East_Asian_Width W (Wide) or F (Fullwidth), from EastAsianWidth.txt,
#       with the unassigned code points the file's header says default to W
#       (U+3400..4DBF, U+4E00..9FFF, U+F900..FAFF, U+20000..2FFFD,
#       U+30000..3FFFD) unless the file lists them otherwise;
#    0  General_Category Mn or Me (non-spacing and enclosing combining marks),
#       from extracted/DerivedGeneralCategory.txt, and gweni's default-
#       ignorables (U+200B..200D, U+2060, U+FE00..FE0F, U+FEFF, which every
#       text walk skips: freetype-gl's utf8_is_ignorable); a zero-width code
#       point that is also W counts 0;
#    1  everything else, Ambiguous (A) included.
#
#  Both files are of one Unicode version, downloaded from unicode.org or read
#  from a directory holding them:
#
#      tools/gen_cell_widths.py                      # VERSION below, downloaded
#      tools/gen_cell_widths.py --ucd DIR            # DIR/EastAsianWidth.txt and
#                                                    # DIR/DerivedGeneralCategory.txt
#      tools/gen_cell_widths.py --version 18.0.0 --out FILE
#
#  The output names the version and is the same bytes for the same files, so
#  regenerating on an unchanged version changes nothing.
#

import argparse
import os
import sys
import urllib.request

VERSION = "18.0.0"
URL = "https://www.unicode.org/Public/{version}/ucd/{path}"

# East_Asian_Width's default W for unassigned code points (EastAsianWidth.txt's header)
DEFAULT_WIDE = [(0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xF900, 0xFAFF),
                (0x20000, 0x2FFFD), (0x30000, 0x3FFFD)]
# gweni's default-ignorables (deps/freetype-gl utf8_is_ignorable)
IGNORABLE = [(0x200B, 0x200D), (0x2060, 0x2060), (0xFE00, 0xFE0F), (0xFEFF, 0xFEFF)]


def read_ucd(args, name, path):
    if args.ucd:
        with open(os.path.join(args.ucd, name), encoding="utf-8") as f:
            return f.read()
    url = URL.format(version=args.version, path=path)
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read().decode("utf-8")


def parse(text):
    """(first, last, value) for each data line, and the version the header names"""
    version = None
    rows = []
    for line in text.splitlines():
        if version is None and line.startswith("# ") and line.endswith(".txt") and "-" in line:
            version = line[2:-4].rsplit("-", 1)[1]
        data = line.split("#", 1)[0].strip()
        if not data:
            continue
        cps, value = [field.strip() for field in data.split(";")[:2]]
        first, _, last = cps.partition("..")
        rows.append((int(first, 16), int(last or first, 16), value))
    return version, rows


def merge(ranges):
    merged = []
    for first, last in sorted(ranges):
        if merged and first <= merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], max(merged[-1][1], last))
        else:
            merged.append((first, last))
    return merged


def subtract(ranges, holes):
    """ranges less every code point in holes (both merged)"""
    out = []
    for first, last in ranges:
        start = first
        for hole_first, hole_last in holes:
            if hole_last < start or hole_first > last:
                continue
            if hole_first > start:
                out.append((start, hole_first - 1))
            start = max(start, hole_last + 1)
        if start <= last:
            out.append((start, last))
    return out


def table(name, ranges):
    lines = ["static const CellRange %s[] = {" % name]
    for i in range(0, len(ranges), 4):
        row = ", ".join("{ 0x%05X, 0x%05X }" % r for r in ranges[i:i + 4])
        lines.append("    %s," % row)
    lines.append("};")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate the cell width tables of gweni::text::cellCount")
    parser.add_argument("--version", default=VERSION, help="Unicode version to download (default %(default)s)")
    parser.add_argument("--ucd", help="directory holding EastAsianWidth.txt and DerivedGeneralCategory.txt")
    parser.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                      "..", "source", "text", "cellWidthTable.inc"))
    args = parser.parse_args()

    eaw_version, eaw = parse(read_ucd(args, "EastAsianWidth.txt", "EastAsianWidth.txt"))
    gc_version, gc = parse(read_ucd(args, "DerivedGeneralCategory.txt", "extracted/DerivedGeneralCategory.txt"))
    if eaw_version != gc_version:
        sys.exit("gen_cell_widths.py: EastAsianWidth.txt is %s, DerivedGeneralCategory.txt %s"
                 % (eaw_version, gc_version))

    listed = merge([(first, last) for first, last, _ in eaw])
    wide = [(first, last) for first, last, value in eaw if value in ("W", "F")]
    # the defaults hold only where the file lists nothing
    for first, last in DEFAULT_WIDE:
        wide += subtract([(first, last)], listed)
    zero = merge([(first, last) for first, last, value in gc if value in ("Mn", "Me")] + IGNORABLE)
    wide = subtract(merge(wide), zero)

    out = "\n".join([
        "// Generated by tools/gen_cell_widths.py from the Unicode Character Database",
        "// %s (EastAsianWidth.txt, extracted/DerivedGeneralCategory.txt). Do not edit:" % eaw_version,
        "// run the script. See include/gweni/text/cells.h.",
        "",
        "#define GWENI_CELL_WIDTH_UNICODE_VERSION \"%s\"" % eaw_version,
        "",
        "// 0 cells: General_Category Mn and Me, and gweni's default-ignorables",
        table("zeroCellRanges", zero),
        "",
        "// 2 cells: East_Asian_Width W and F, less the zero-cell code points",
        table("wideCellRanges", wide),
        "",
    ])
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(out)
    print("%s: Unicode %s, %d zero-cell and %d wide ranges" % (args.out, eaw_version, len(zero), len(wide)))


if __name__ == "__main__":
    main()
