#!/usr/bin/env python3
"""Join -fdepth-census map records to the runtime [depth] dump.

The compiler writes one [dfn] line per function body into MCC_DEPTH_CENSUS_MAP;
the instrumented binary writes one [depth] line per function into
MCC_DEPTH_CENSUS at exit, carrying the observed maximum live depth and the
per-level call counts.  A level's call count divided by the number of root
invocations is the frontier width at that level, which is the lane count a
wavefront dispatch of that level would have.

The width profile is what prices a recursion.  It is deliberately not turned
into a linear-vs-tree verdict here: a hand-drawn boundary is a cliff, and the
scheduler wants a number it can compare against the CPU, not a category.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

DFN = re.compile(r"^\[dfn\] id=(\d+) fn=(\S+) file=(\S+)$")
DEP = re.compile(
    r"^\[depth\] id=(\d+) calls=(\d+) max=(\d+) roots=(\d+) wmax=(\d+) w=(\S*)$")

# Hand-pinned in tools/loop-census.py and reproduced here so the two copies can
# be cross-checked; its 63+-node rows were measured as noise, so the last two
# entries are carried but flagged rather than trusted.
BREAKEVEN = [(3, 322), (7, 108), (15, 48), (31, 24), (63, 23), (127, 8)]
BREAKEVEN_NOISY = {63, 127}


def breakeven_lanes(nodes):
    best = BREAKEVEN[0][1]
    for n, lanes in BREAKEVEN:
        if nodes >= n:
            best = lanes
    return best


def parse_map(path):
    out = {}
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = DFN.match(line.strip())
            if m:
                out[int(m.group(1))] = (m.group(2), m.group(3))
    return out


def parse_dump(path):
    out = []
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = DEP.match(line.strip())
            if not m:
                continue
            w = [int(x) for x in m.group(6).split(",") if x] if m.group(6) else []
            out.append({
                "id": int(m.group(1)),
                "calls": int(m.group(2)),
                "max": int(m.group(3)),
                "roots": int(m.group(4)),
                "wmax": int(m.group(5)),
                "w": w,
            })
    return out


def widths(rec):
    roots = rec["roots"] or 1
    return [(c + roots - 1) // roots for c in rec["w"]]


def run_one(mcc, src, work, cflags):
    base = os.path.basename(src)[:-2]
    mapf = os.path.join(work, base + ".map")
    dumpf = os.path.join(work, base + ".dump")
    exe = os.path.join(work, base + ".exe")
    env = dict(os.environ)
    env["MCC_DEPTH_CENSUS_MAP"] = mapf
    cmd = [mcc, "-fdepth-census", "-w"] + cflags + [src, "-o", exe]
    try:
        r = subprocess.run(cmd, env=env, capture_output=True, timeout=120)
    except subprocess.TimeoutExpired:
        return None
    if r.returncode != 0 or not os.path.exists(exe):
        return None
    env2 = dict(os.environ)
    env2["MCC_DEPTH_CENSUS"] = dumpf
    try:
        subprocess.run([exe], env=env2, capture_output=True, timeout=60)
    except subprocess.TimeoutExpired:
        return None
    if not os.path.exists(dumpf) or not os.path.exists(mapf):
        return None
    return parse_map(mapf), parse_dump(dumpf)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mcc", required=True)
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--work", default=None)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--cflags", default="-O1")
    ap.add_argument("--min-depth", type=int, default=2)
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    work = a.work or tempfile.mkdtemp(prefix="depthcensus")
    os.makedirs(work, exist_ok=True)
    cflags = [x for x in a.cflags.split(",") if x]

    if os.path.isdir(a.corpus):
        srcs = sorted(
            os.path.join(a.corpus, f) for f in os.listdir(a.corpus)
            if f.endswith(".c"))
    else:
        srcs = [a.corpus]
    if a.limit:
        srcs = srcs[:a.limit]

    progs = 0
    recs = []
    for s in srcs:
        got = run_one(a.mcc, s, work, cflags)
        if not got:
            continue
        progs += 1
        fnmap, dump = got
        for rec in dump:
            if rec["max"] < a.min_depth:
                continue
            name, loc = fnmap.get(rec["id"], ("?", "?"))
            rec["fn"] = name
            rec["loc"] = loc
            rec["src"] = os.path.basename(s)
            recs.append(rec)

    if not recs:
        print("depth-census: programs=%d recursive-fns=0" % progs)
        return 0

    w1 = [r for r in recs if r["wmax"] <= 1]
    wide = [r for r in recs if r["wmax"] > 1]
    depths = sorted(r["max"] for r in recs)
    wmaxes = sorted(r["wmax"] for r in recs)

    def pct(v, p):
        return v[min(len(v) - 1, int(len(v) * p / 100.0))]

    print("depth-census: programs=%d recursive-fns=%d width1=%d widthN=%d" %
          (progs, len(recs), len(w1), len(wide)))
    print("depth-census: depth min=%d p50=%d p90=%d max=%d" %
          (depths[0], pct(depths, 50), pct(depths, 90), depths[-1]))
    print("depth-census: wmax min=%d p50=%d p90=%d max=%d" %
          (wmaxes[0], pct(wmaxes, 50), pct(wmaxes, 90), wmaxes[-1]))

    clears = 0
    for r in recs:
        if r["wmax"] >= breakeven_lanes(3):
            clears += 1
    print("depth-census: fns-whose-wmax-clears-widest-breakeven=%d/%d" %
          (clears, len(recs)))

    if not a.quiet:
        for r in sorted(recs, key=lambda r: -r["wmax"])[:20]:
            print("  %-24s %-28s depth=%-3d wmax=%-6d w=%s" %
                  (r["src"], r["fn"], r["max"], r["wmax"],
                   ",".join(str(x) for x in widths(r)[:12])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
