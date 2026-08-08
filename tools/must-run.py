#!/usr/bin/env python3
"""Enforce tests/must-run.txt (N13).

Two checks, deliberately separated because they fail for different reasons and
want different audiences:

  --build DIR      every `registered` and `must-run` row names a cell that this
                   build actually registered. Catches the matrix.yml class of
                   bug, where a missing dependency quietly removes cells and the
                   suite still reports success.

  --results FILE   every `must-run` row names a cell that did not report Skipped
                   in this ctest JUnit XML. Only meaningful on a host that was
                   supposed to be able to run them, so it is opt-in -- and only
                   against a FULL-suite results file, since a `-R` subset will
                   legitimately not contain most rows.

Exit 0 clean, 1 on any violation, 2 on a usage or parse problem. Never 77: a
manifest that cannot be checked is a failure, not a skip -- that is the whole
point of the file.
"""

import argparse
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

MODES = ("registered", "must-run")


def load(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) != 3:
                sys.stderr.write(
                    "%s:%d: expected 3 `|`-separated columns, got %d\n"
                    % (path, lineno, len(parts)))
                sys.exit(2)
            name, mode, why = parts
            if mode not in MODES:
                sys.stderr.write("%s:%d: unknown mode %r (want one of %s)\n"
                                 % (path, lineno, mode, ", ".join(MODES)))
                sys.exit(2)
            rows.append((name, mode, why))
    if not rows:
        sys.stderr.write("%s: the manifest is empty, so it asserts nothing\n" % path)
        sys.exit(2)
    return rows


def registered_tests(build):
    out = subprocess.run(["ctest", "--test-dir", build, "-N"],
                         capture_output=True, text=True)
    if out.returncode != 0:
        sys.stderr.write("ctest -N failed in %s:\n%s\n" % (build, out.stderr))
        sys.exit(2)
    names = set()
    for line in out.stdout.splitlines():
        m = re.match(r"\s*Test\s+#\d+:\s+(\S.*?)\s*$", line)
        if m:
            names.add(m.group(1))
    if not names:
        sys.stderr.write("ctest -N listed no tests at all in %s; refusing to "
                         "report a clean manifest against an empty build\n" % build)
        sys.exit(2)
    return names


def skipped_tests(path):
    try:
        root = ET.parse(path).getroot()
    except Exception as exc:                                  # noqa: BLE001
        sys.stderr.write("cannot parse %s: %s\n" % (path, exc))
        sys.exit(2)
    seen, skipped = set(), set()
    for case in root.iter("testcase"):
        name = case.get("name")
        if not name:
            continue
        seen.add(name)
        if case.find("skipped") is not None or case.get("status") == "notrun":
            skipped.add(name)
    if not seen:
        sys.stderr.write("%s records no testcase elements\n" % path)
        sys.exit(2)
    return seen, skipped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=None)
    ap.add_argument("--build", default=None)
    ap.add_argument("--results", default=None)
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    manifest = args.manifest or os.path.join(here, os.pardir, "tests",
                                             "must-run.txt")
    rows = load(manifest)

    if not args.build and not args.results:
        sys.stderr.write("nothing to check: pass --build and/or --results\n")
        sys.exit(2)

    bad = []

    if args.build:
        have = registered_tests(args.build)
        for name, _mode, why in rows:
            if name not in have:
                bad.append("NOT REGISTERED  %-30s  %s" % (name, why))

    if args.results:
        seen, skipped = skipped_tests(args.results)
        for name, mode, why in rows:
            if mode != "must-run":
                continue
            if name not in seen:
                bad.append("NOT RUN         %-30s  %s" % (name, why))
            elif name in skipped:
                bad.append("SKIPPED         %-30s  %s" % (name, why))

    if bad:
        sys.stderr.write("must-run: %d violation(s)\n" % len(bad))
        for b in bad:
            sys.stderr.write("  %s\n" % b)
        sys.stderr.write(
            "\nA cell named here is one the suite is not allowed to lose "
            "silently.\nIf it genuinely cannot exist on this host, say so by "
            "editing %s.\n" % manifest)
        return 1

    print("must-run: %d row(s) satisfied" % len(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
