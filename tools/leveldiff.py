#!/usr/bin/env python3

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

UNUSABLE = ("cfail", "ctimeout", "rtimeout")


def parse_known(path):
    known = {}
    if not path:
        return known
    with open(path) as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split("|", 2)]
            if len(parts) != 3:
                sys.stderr.write("leveldiff: %s:%d: want 3 '|' columns, got %d\n"
                                 % (path, lineno, len(parts)))
                sys.exit(2)
            prog, spec, why = parts
            if not why:
                sys.stderr.write("leveldiff: %s:%d: a known row needs a reason\n" % (path, lineno))
                sys.exit(2)
            lv = set()
            for chunk in spec.split(","):
                chunk = chunk.strip()
                if "-" in chunk:
                    lo, hi = chunk.split("-", 1)
                    lv.update(range(int(lo), int(hi) + 1))
                else:
                    lv.add(int(chunk))
            known[prog] = (lv, why)
    return known


def one(mcc, corpus, work, cflags, src, lvl, timeout):
    base = src[:-2]
    exe = os.path.join(work, "%s.O%d" % (base, lvl))
    cmd = [mcc] + cflags + ["-O%d" % lvl, os.path.join(corpus, src), "-o", exe]
    try:
        p = subprocess.run(cmd, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return ("ctimeout", "")
    if p.returncode != 0:
        return ("cfail", "")
    try:
        r = subprocess.run([exe], capture_output=True, timeout=timeout, cwd=work)
        out = ("rc%d" % r.returncode, hashlib.sha256(r.stdout).hexdigest()[:16])
    except subprocess.TimeoutExpired:
        out = ("rtimeout", "")
    try:
        os.unlink(exe)
    except OSError:
        pass
    return out


def sweep(mcc, corpus, work, cflags, srcs, lvl, jobs, timeout):
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        res = list(ex.map(lambda s: one(mcc, corpus, work, cflags, s, lvl, timeout), srcs))
    return dict(zip(srcs, res))


def main(argv):
    ap = argparse.ArgumentParser(
        description="compile and run a C corpus at each -O level and diff exit code "
                    "and stdout against the -O0 answer for the same program")
    ap.add_argument("--mcc", required=True, help="the compiler under test")
    ap.add_argument("--corpus", required=True, help="directory of self-checking .c programs")
    ap.add_argument("--work", required=True, help="scratch directory for objects and binaries")
    ap.add_argument("--levels", default="0,1,2,3,4",
                    help="comma-separated -O levels; the first is the reference")
    ap.add_argument("--known", default="", help="table of divergences that predate this cell")
    ap.add_argument("--cflags", default="-w,-std=gnu11,-lm", help="comma-separated flags")
    ap.add_argument("--jobs", type=int, default=32, help="parallel compile+run fan-out")
    ap.add_argument("--timeout", type=int, default=20, help="per-program seconds")
    ap.add_argument("--minprog", type=int, default=1200,
                    help="floor on programs that must reach the reference sweep, so that "
                         "zero divergences cannot mean zero programs compared")
    ap.add_argument("--mutate", default="",
                    help="perturb the recorded reference answer for this program; the run "
                         "must then report it as an unknown divergence")
    args = ap.parse_args(argv[1:])

    levels = [int(x) for x in args.levels.split(",")]
    cflags = [f for f in args.cflags.split(",") if f]
    known = parse_known(args.known)

    if not os.path.isdir(args.corpus):
        print("SKIP: no corpus at %s" % args.corpus)
        return 77
    srcs = sorted(f for f in os.listdir(args.corpus) if f.endswith(".c"))
    if not srcs:
        print("SKIP: no .c programs in %s" % args.corpus)
        return 77

    shutil.rmtree(args.work, ignore_errors=True)
    os.makedirs(args.work, exist_ok=True)

    t0 = time.time()
    ref = sweep(args.mcc, args.corpus, args.work, cflags, srcs, levels[0], args.jobs, args.timeout)
    subjects = [s for s in srcs if ref[s][0] not in UNUSABLE]
    print("leveldiff: corpus=%d reference=-O%d subjects=%d unusable-at-reference=%d (%.1fs)"
          % (len(srcs), levels[0], len(subjects), len(srcs) - len(subjects), time.time() - t0))

    if len(subjects) < args.minprog:
        print("FAIL: only %d of %d programs compiled and ran at -O%d, floor is %d; a "
              "differential with no subjects reports clean for the wrong reason"
              % (len(subjects), len(srcs), levels[0], args.minprog))
        return 1

    if args.mutate:
        if args.mutate not in ref:
            print("FAIL: --mutate names %s, which is not in the corpus" % args.mutate)
            return 1
        if args.mutate not in subjects:
            print("FAIL: --mutate names %s, which the reference sweep could not use" % args.mutate)
            return 1
        rc, dig = ref[args.mutate]
        ref[args.mutate] = ("rc%d" % (int(rc[2:]) + 1), dig)
        print("leveldiff: mutated the recorded -O%d answer for %s: %s -> %s"
              % (levels[0], args.mutate, rc, ref[args.mutate][0]))

    seen = {}
    bad = 0
    for lvl in levels[1:]:
        t1 = time.time()
        got = sweep(args.mcc, args.corpus, args.work, cflags, srcs, lvl, args.jobs, args.timeout)
        rows = []
        for s in subjects:
            if got[s] == ref[s]:
                continue
            seen.setdefault(s, set()).add(lvl)
            lv, why = known.get(s, (set(), ""))
            rows.append((s, got[s], lvl in lv, why))
        unknown = [r for r in rows if not r[2]]
        bad += len(unknown)
        print("leveldiff: -O%-2d %4d divergences, %d known, %d unknown (%.1fs)"
              % (lvl, len(rows), len(rows) - len(unknown), len(unknown), time.time() - t1))
        for s, g, isknown, why in rows:
            mark = "known" if isknown else "NEW  "
            print("    %s %-26s -O%d=%s/%s  -O%d=%s/%s%s"
                  % (mark, s, levels[0], ref[s][0], ref[s][1], lvl, g[0], g[1],
                     "  [%s]" % why if isknown else ""))

    stale = 0
    for prog, (lv, why) in sorted(known.items()):
        if prog not in [s for s in srcs]:
            print("STALE: %s is listed as a known divergence but is not in the corpus" % prog)
            stale += 1
            continue
        gone = sorted(l for l in lv if l in levels[1:] and l not in seen.get(prog, set()))
        if gone:
            print("STALE: %s no longer diverges at %s; drop those levels from the known table"
                  % (prog, ",".join("-O%d" % l for l in gone)))
            stale += 1

    shutil.rmtree(args.work, ignore_errors=True)
    print("leveldiff: %d level(s) compared, %d unknown divergence(s), %d stale known row(s)"
          % (len(levels) - 1, bad, stale))
    return 1 if (bad or stale) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
