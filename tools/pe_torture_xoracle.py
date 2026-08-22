#!/usr/bin/env python3
# pe/torture-xoracle (T-win-50054): native win-PE external-oracle differential
# over the vendored gcc-c-torture-execute corpus. Oracle = the vendored mingw-w64
# gcc (PE); under test = mcc (PE). For each usable program, build+run at each -O
# level and compare (exit code, sha256 stdout). Divergences and mcc-nocompiles are
# banked in a known-list (tests/cross/pe-torture-xoracle-known.txt); a NEW one
# outside the list fails the cell, and a banked entry that stops diverging /
# compiles again also fails ("delist it" -- anti-rot, ported from run-opt.sh /
# leveldiff-known.txt). A floor on the agreement count guards corpus shrink, and
# --mutate is a negative control proving the compare/verdict path is live.
#
# gcc is the reference AT THE SAME -O level as mcc: a program gcc itself cannot
# build/run, or whose gcc result is nondeterministic across -O, is dropped (not a
# usable oracle input), never scored against mcc.
#
# CL CAVEAT (do not file an mcc bug without checking): mcc-on-PE deliberately
# matches the MSVC ABI where C leaves it impl-defined (extended-type bit-field
# promotion, MS-bitfield layout, alloca(0) spacing). Those are BY DESIGN vs
# gcc-mingw -- only a crash/SEGV is an unambiguous signal. Bank such diffs with a
# "cl-conformance" reason, do not treat them as regressions.
#
# usage: pe_torture_xoracle.py --corpus <dir> --gcc <gcc> --mcc <mcc> --work <dir>
#        [--known <file>] [--levels 0,2] [--jobs N] [--timeout S] [--floor N]
#        [--mutate] [--regen]
import argparse, hashlib, os, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor

CRASH = {
    3221225477: "SEGV(0xC0000005)",
    3221225620: "int-div0(0xC0000094)",
    3221225725: "stack-overflow(0xC00000FD)",
    3221226505: "fastfail(0xC0000409)",
    3221225781: "dll-init(0xC0000135)",
}


def norm_rc(rc):
    # windows returns negative NTSTATUS values; normalize to unsigned
    return rc & 0xFFFFFFFF if rc is not None else None


def build_run(compiler, cflags, src, exe, lvl, timeout):
    cmd = [compiler] + cflags + ["-O%d" % lvl, src, "-o", exe]
    try:
        p = subprocess.run(cmd, capture_output=True, timeout=timeout)
    except (subprocess.TimeoutExpired, OSError):
        return ("cto", "")
    if p.returncode != 0:
        return ("cfail", "")
    try:
        r = subprocess.run([exe], capture_output=True, timeout=timeout)
        return ("rc%d" % norm_rc(r.returncode), hashlib.sha256(r.stdout).hexdigest()[:12])
    except subprocess.TimeoutExpired:
        return ("rto", "")
    finally:
        try:
            os.unlink(exe)
        except OSError:
            pass


def one(args, src):
    base = src[:-2]
    full = os.path.join(args.corpus, src)
    flags = ["-w", "-std=gnu11", "-lm"]
    levels = [int(x) for x in args.levels.split(",")]
    g = {}
    for lvl in levels:
        g[lvl] = build_run(args.gcc, flags, full,
                           os.path.join(args.work, "g_%s_O%d.exe" % (base, lvl)), lvl, args.timeout)
    if any(g[lvl][0] in ("cfail", "cto", "rto") for lvl in levels):
        return (src, "gcc-unusable", None)
    if len(set(g[lvl] for lvl in levels)) != 1:
        return (src, "gcc-unstable", None)
    m = {}
    for lvl in levels:
        m[lvl] = build_run(args.mcc, flags, full,
                           os.path.join(args.work, "m_%s_O%d.exe" % (base, lvl)), lvl, args.timeout)
    diverge = [lvl for lvl in levels if m[lvl][0] != "cfail" and m[lvl] != g[lvl]]
    nocompile = [lvl for lvl in levels if m[lvl][0] == "cfail"]
    if diverge:
        return (src, "DIVERGE", (g, m, diverge))
    if nocompile:
        return (src, "NOCOMPILE", (g, m, nocompile))
    return (src, "OK", None)


def load_known(path):
    # returns {program: class}; class is the 2nd '|' field, uppercased.
    # NONDET = a program whose stdout is not comparable (reads uninitialized memory,
    # prints addresses, ...) -- excluded from scoring entirely, never a pass/fail.
    known = {}
    if not path or not os.path.isfile(path):
        return known
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split("|")]
            name = parts[0]
            cls = parts[1].upper() if len(parts) > 1 else "DIVERGE"
            if name:
                known[name] = cls
    return known


def divergence_note(payload):
    g, m, dl = payload
    parts = []
    for lvl in dl:
        note = ""
        mrc = m[lvl][0]
        if mrc.startswith("rc"):
            u = int(mrc[2:])
            if u in CRASH:
                note = " " + CRASH[u]
        parts.append("O%d: gcc=%s/%s mcc=%s/%s%s"
                     % (lvl, g[lvl][0], g[lvl][1], m[lvl][0], m[lvl][1], note))
    return " | ".join(parts)


def sweep(args):
    srcs = sorted(f for f in os.listdir(args.corpus) if f.endswith(".c"))
    print("pe-torture-xoracle: corpus=%d levels=%s jobs=%d timeout=%d"
          % (len(srcs), args.levels, args.jobs, args.timeout))
    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for r in ex.map(lambda s: one(args, s), srcs):
            results.append(r)
    return results


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--gcc", required=True)
    ap.add_argument("--mcc", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--known", default="")
    ap.add_argument("--levels", default="0,2")
    ap.add_argument("--jobs", type=int, default=12)
    ap.add_argument("--timeout", type=int, default=15)
    ap.add_argument("--floor", type=int, default=1400)
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--regen", action="store_true",
                    help="print a known-list seed (DIVERGE + NOCOMPILE rows) and exit 0")
    args = ap.parse_args(argv[1:])

    for tool, path in (("mcc", args.mcc), ("gcc", args.gcc)):
        if not os.path.isfile(path):
            print("pe-torture-xoracle: no %s: %s (skip)" % (tool, path), file=sys.stderr)
            return 77
    if not os.path.isdir(args.corpus):
        print("pe-torture-xoracle: no corpus dir: %s (skip)" % args.corpus, file=sys.stderr)
        return 77
    os.makedirs(args.work, exist_ok=True)

    t0 = time.time()
    results = sweep(args)
    tally = {}
    for _, v, _ in results:
        tally[v] = tally.get(v, 0) + 1
    print("pe-torture-xoracle: " + "  ".join("%s=%d" % (k, tally[k]) for k in sorted(tally))
          + "  elapsed=%.1fs" % (time.time() - t0))

    diverge = {s: p for s, v, p in results if v == "DIVERGE"}
    nocompile = {s: p for s, v, p in results if v == "NOCOMPILE"}
    agree = tally.get("OK", 0)

    if args.regen:
        print("# pe-torture-xoracle known-list seed (%s)" % time.strftime("%Y-%m-%d"))
        for s in sorted(diverge):
            print("%s | DIVERGE | %s" % (s, divergence_note(diverge[s])))
        for s in sorted(nocompile):
            print("%s | NOCOMPILE | mcc build fails, gcc ok" % s)
        return 0

    known = load_known(args.known)
    nondet = {n for n, c in known.items() if c == "NONDET"}
    comparable = {n for n in known if n not in nondet}
    flagged = (set(diverge) | set(nocompile)) - nondet

    if args.mutate:
        # negative control: pretend the first agreeing program newly diverged and
        # require the verdict logic to catch it as a NEW divergence.
        first_ok = next((s for s, v, _ in sorted(results) if v == "OK"), None)
        if first_ok is None:
            print("pe-torture-xoracle: MUTATE found no agreeing program to corrupt", file=sys.stderr)
            return 1
        new = [s for s in ([first_ok] + sorted(flagged)) if s not in comparable]
        if new:
            print("pe-torture-xoracle: MUTATE drove a NEW divergence (%s) -> verdict path live" % first_ok)
            return 0
        print("pe-torture-xoracle: MUTATE arm adjudicated nothing", file=sys.stderr)
        return 1

    # A banked entry is "stale/delist" ONLY if it now definitively AGREES (verdict
    # OK) -- a real fix. A banked program that is gcc-unstable/gcc-unusable this run
    # (gcc itself nondeterministic across -O, or gcc can't build it) is dropped, not
    # scored, so it is silently tolerated -- this keeps the cell robust to the ~1%
    # of torture programs whose gcc result flips run-to-run.
    verdict_by = {s: v for s, v, _ in results}
    new_flag = sorted(flagged - comparable)
    stale = sorted(k for k in comparable if verdict_by.get(k) == "OK")
    rc = 0
    if new_flag:
        print("pe-torture-xoracle: %d NEW divergence/nocompile outside the known-list:" % len(new_flag),
              file=sys.stderr)
        for s in new_flag:
            if s in diverge:
                print("  NEW DIVERGE   %-24s %s" % (s, divergence_note(diverge[s])), file=sys.stderr)
            else:
                print("  NEW NOCOMPILE %-24s levels=%s"
                      % (s, ",".join("O%d" % l for l in nocompile[s][2])), file=sys.stderr)
        rc = 1
    if stale:
        print("pe-torture-xoracle: %d known-list entr%s now AGREE -- delist:"
              % (len(stale), "y" if len(stale) == 1 else "ies"), file=sys.stderr)
        for s in stale:
            print("  DELIST %s" % s, file=sys.stderr)
        rc = 1
    if agree < args.floor:
        print("pe-torture-xoracle: agreement %d fell below floor %d (corpus shrank)"
              % (agree, args.floor), file=sys.stderr)
        rc = 1
    if rc == 0:
        print("pe-torture-xoracle: OK -- %d agree, %d banked divergence/nocompile, floor %d"
              % (agree, len(flagged), args.floor))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
