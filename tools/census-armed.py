#!/usr/bin/env python3
"""Every `census`-labelled cell is armed, so the label cannot report a skip as a pass.

Each census tool refuses to run unless an environment switch is set, and prints
what to set before returning 77.  That is a courtesy to whoever types the tool
by hand; it turned into a hole the moment the cells were registered, because
nothing in CMakeLists.txt set the switches.  `MCC_RIR_CENSUS=1 ctest -L census`
then printed `100% tests passed ... out of 6` with three of the six Skipped,
and `slice-census` sat in tests/must-run.txt as `registered` -- green on a cell
that had never once executed.

The check is structural, not a list.  For every test carrying the `census`
label it reads the tool named on the command line, recovers the env var that
tool's own opt-in guard consults, and requires the cell's ENVIRONMENT property
to set it to something other than empty or `0`.  A census cell added tomorrow
with a new switch is caught by the same rule, and a switch renamed in the tool
but not in CMakeLists.txt is caught as a mismatch rather than as a silent skip.

Exit 0 clean, 1 on any violation, 2 on a usage or parse problem.  Never 77:
this cell exists to make a skip visible, so it must not be able to skip.
"""

import argparse
import json
import os
import re
import subprocess
import sys

GUARD = re.compile(r"\.opt_in\s+and\s+not\s+os\.environ\.get\(\s*[\"']([A-Za-z_][A-Za-z_0-9]*)[\"']")


def show_only(build):
    out = subprocess.run(["ctest", "--test-dir", build, "-N", "-L", "census",
                          "--show-only=json-v1"], capture_output=True, text=True)
    if out.returncode != 0:
        sys.stderr.write("ctest --show-only failed in %s:\n%s\n" % (build, out.stderr))
        sys.exit(2)
    try:
        return json.loads(out.stdout)
    except ValueError as exc:
        sys.stderr.write("cannot parse ctest --show-only=json-v1: %s\n" % exc)
        sys.exit(2)


def props(test):
    return {p["name"]: p["value"] for p in test.get("properties", [])}


def env_map(value):
    out = {}
    for entry in value or []:
        k, _, v = str(entry).partition("=")
        out[k] = v
    return out


def guard_var(path):
    try:
        txt = open(path, encoding="utf-8", errors="replace").read()
    except OSError as exc:
        return None, "cannot read %s: %s" % (path, exc)
    found = GUARD.findall(txt)
    if not found:
        return None, ("%s takes --opt-in but no `a.opt_in and not "
                      "os.environ.get(...)` guard was found in it, so this "
                      "check cannot tell what would arm it"
                      % os.path.basename(path))
    if len(set(found)) > 1:
        return None, ("%s consults %s in its opt-in guard; a cell can only be "
                      "proved armed against one switch"
                      % (os.path.basename(path), ", ".join(sorted(set(found)))))
    return found[0], None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("--min-cells", type=int, default=1)
    a = ap.parse_args()

    data = show_only(a.build)
    tests = [t for t in data.get("tests", [])
             if "census" in props(t).get("LABELS", [])]
    if len(tests) < a.min_cells:
        sys.stderr.write("census-armed: `ctest -L census` selects %d cell(s), "
                         "expected at least %d. A label that selects nothing "
                         "cannot be reported as passing\n"
                         % (len(tests), a.min_cells))
        return 1

    bad, checked = [], 0
    for t in tests:
        cmd = t.get("command", [])
        if "--opt-in" not in cmd:
            continue
        tools = [c for c in cmd if c.endswith(".py")]
        if len(tools) != 1:
            bad.append("%s: passes --opt-in but names %d .py tools on its "
                       "command line" % (t["name"], len(tools)))
            continue
        var, why = guard_var(tools[0])
        if why:
            bad.append("%s: %s" % (t["name"], why))
            continue
        checked += 1
        env = env_map(props(t).get("ENVIRONMENT"))
        got = env.get(var)
        if got is None:
            bad.append("%s: %s gates it on %s, and the cell's ENVIRONMENT does "
                       "not set it. The cell will report Skipped and ctest will "
                       "count it as passed"
                       % (t["name"], os.path.basename(tools[0]), var))
        elif got in ("", "0"):
            bad.append("%s: ENVIRONMENT sets %s=%r, which the tool reads as "
                       "unset" % (t["name"], var, got))

    if not checked and not bad:
        sys.stderr.write("census-armed: none of the %d census cell(s) passes "
                         "--opt-in, so nothing was proved. Either the label "
                         "moved or the opt-in convention did\n" % len(tests))
        return 1

    for b in bad:
        print("census-armed: " + b)
    if bad:
        print("census-armed: %d of %d opt-in census cell(s) unarmed" %
              (len(bad), checked + len(bad)))
        return 1
    print("census-armed: OK -- %d census cell(s), %d gated on an opt-in switch, "
          "all armed by CMakeLists.txt" % (len(tests), checked))
    return 0


if __name__ == "__main__":
    sys.exit(main())
