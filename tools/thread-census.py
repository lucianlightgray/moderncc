#!/usr/bin/env python3
"""The threading-to-dependency census, and its ground truth.

`mcc -O1` with MCC_THREAD_CENSUS set emits three record kinds:

    [throp]  fn=<f> op=<primitive> supported=<0|1>
    [thrsec] fn=<f> lock=<key> id=<n> stmts=<n> bases=<n> opaque=<0|1> ovf=<0|1>
    [thrpair] lock=<key> a=<id> b=<id> verdict=<independent|conflict|unknown> why=<...>

A [thrsec] is one lock/unlock critical section; a [thrpair] is the engine's
answer to "may these two regions guarded by the same lock run concurrently".
That answer is what decides whether the mutex becomes a serializing edge or
nothing at all, so the cell that matters is the unsoundness one: a pair that
genuinely conflicts must never come back `independent`.

--selftest compiles tests/thread/locks.c, whose sections are conflicting or
disjoint by construction, and checks the verdicts against the table below,
with a negative control (-O0 builds no AST, so nothing is emitted) and a
perturbation (make one disjoint section write the other's object, and the
verdict must flip).  Without --selftest it runs the census over the files named
on the command line and reports the independence rate and the blocking causes.

Exit 0 clean, 1 on a violation, 2 on usage, 77 when the compiler is missing.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

EXPECT = {
    ("sec_a_ga", "sec_a_gb"): "independent",
    ("sec_a_ga", "sec_a_ga_again"): "conflict",
    ("sec_a_gb", "sec_a_ga_again"): "independent",
    ("sec_a_ga", "sec_a_indirect"): "unknown",
    ("sec_a_gb", "sec_a_indirect"): "unknown",
    ("sec_a_ga_again", "sec_a_indirect"): "unknown",
    ("sec_a_ga", "sec_a_opaque"): "unknown",
    ("sec_a_gb", "sec_a_opaque"): "unknown",
    ("sec_a_ga_again", "sec_a_opaque"): "unknown",
    ("sec_a_indirect", "sec_a_opaque"): "unknown",
    ("sec_b_gc", "sec_b_gd"): "independent",
    ("sec_b_gc", "sec_b_arr"): "independent",
    ("sec_b_gd", "sec_b_arr"): "conflict",
}

PERTURB = {("sec_a_ga", "sec_a_gb"): "conflict",
           ("sec_a_gb", "sec_a_ga_again"): "conflict"}

SEC = re.compile(r"\[thrsec\] fn=(\S+) lock=(\d+) id=(\d+) stmts=(\d+) "
                 r"bases=(\d+) opaque=(\d+) ovf=(\d+)")
PAIR = re.compile(r"\[thrpair\] lock=(\d+) a=(\d+) b=(\d+) verdict=(\S+) why=(\S+)")
OP = re.compile(r"\[throp\] fn=(\S+) op=(\S+) supported=(\d+)")


def census(mcc, src, level, defines=(), includes=()):
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "census.txt")
        cmd = [mcc, "-" + level, "-c", src, "-o", os.path.join(d, "a.o")]
        for x in defines:
            cmd.append("-D" + x)
        for x in includes:
            cmd.append("-I" + x)
        env = dict(os.environ)
        env["MCC_THREAD_CENSUS"] = out
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        text = ""
        if os.path.exists(out):
            with open(out) as f:
                text = f.read()
        return r.returncode, text


def parse(text):
    secs, pairs, ops = {}, [], []
    for line in text.splitlines():
        m = SEC.match(line)
        if m:
            secs[int(m.group(3))] = {
                "fn": m.group(1), "lock": m.group(2), "stmts": int(m.group(4)),
                "bases": int(m.group(5)), "opaque": int(m.group(6)),
                "ovf": int(m.group(7))}
            continue
        m = PAIR.match(line)
        if m:
            pairs.append((int(m.group(2)), int(m.group(3)), m.group(4),
                          m.group(5)))
            continue
        m = OP.match(line)
        if m:
            ops.append((m.group(1), m.group(2), int(m.group(3))))
    named = []
    for a, b, v, why in pairs:
        if a in secs and b in secs:
            named.append((secs[a]["fn"], secs[b]["fn"], v, why))
    return secs, named, ops


def selftest(mcc, root):
    src = os.path.join(root, "tests", "thread", "locks.c")
    if not os.path.exists(src):
        print("thread-census: corpus missing: %s" % src)
        return 2
    bad = 0

    rc, text = census(mcc, src, "O0")
    _, named, _ = parse(text)
    if named:
        print("thread-census: FAIL negative control: -O0 builds no AST yet "
              "%d pair verdicts were emitted" % len(named))
        bad += 1
    else:
        print("thread-census: negative control OK (-O0 emits no verdict)")

    for level in ("O1", "O2"):
        rc, text = census(mcc, src, level)
        if rc != 0:
            print("thread-census: FAIL corpus does not compile at -%s" % level)
            bad += 1
            continue
        secs, named, ops = parse(text)
        seen = {}
        for a, b, v, why in named:
            seen[(a, b)] = (v, why)
        if len(seen) != len(EXPECT):
            print("thread-census: FAIL -%s: %d pairs, expected %d"
                  % (level, len(seen), len(EXPECT)))
            bad += 1
        for key, want in EXPECT.items():
            got = seen.get(key)
            if got is None:
                print("thread-census: FAIL -%s: no verdict for %s/%s"
                      % (level, key[0], key[1]))
                bad += 1
                continue
            if got[0] != want:
                print("thread-census: FAIL -%s: %s/%s -> %s (%s), expected %s"
                      % (level, key[0], key[1], got[0], got[1], want))
                bad += 1
            if want == "conflict" and got[0] == "independent":
                print("thread-census: UNSOUND -%s: %s/%s share an object and "
                      "were reported independent" % (level, key[0], key[1]))
                bad += 1
        if not any(o[1] == "lock" for o in ops):
            print("thread-census: FAIL -%s: no lock primitive was recognised"
                  % level)
            bad += 1
        print("thread-census: -%s %d sections, %d pairs checked"
              % (level, len(secs), len(seen)))

    rc, text = census(mcc, src, "O1", defines=("THREAD_CENSUS_PERTURB",))
    _, named, _ = parse(text)
    seen = {(a, b): v for a, b, v, _ in named}
    for key, want in PERTURB.items():
        got = seen.get(key)
        if got != want:
            print("thread-census: FAIL perturbation: %s/%s -> %s, expected %s "
                  "-- the verdict does not follow the code"
                  % (key[0], key[1], got, want))
            bad += 1
    if not bad:
        print("thread-census: perturbation OK (a disjoint section made to "
              "share an object flips to conflict)")
    return 1 if bad else 0


def sweep(mcc, files, level, includes, defines):
    tot = {"independent": 0, "conflict": 0, "unknown": 0}
    why = {}
    nsec = nfile = nfail = 0
    ops = {}
    for f in files:
        rc, text = census(mcc, f, level, defines=defines,
                          includes=includes + [os.path.dirname(f)])
        if rc != 0:
            nfail += 1
            continue
        nfile += 1
        secs, named, oplist = parse(text)
        nsec += len(secs)
        for _, _, v, w in named:
            tot[v] = tot.get(v, 0) + 1
            why[w] = why.get(w, 0) + 1
        for _, o, sup in oplist:
            ops[o] = ops.get(o, 0) + 1
    n = sum(tot.values())
    print("thread-census: %d file(s) compiled, %d refused, %d critical "
          "section(s), %d pair(s)" % (nfile, nfail, nsec, n))
    for k in sorted(ops, key=lambda x: -ops[x]):
        print("  op %-10s %d" % (k, ops[k]))
    for k in ("independent", "conflict", "unknown"):
        pct = (100.0 * tot.get(k, 0) / n) if n else 0.0
        print("  %-12s %5d  %5.1f%%" % (k, tot.get(k, 0), pct))
    for k in sorted(why, key=lambda x: -why[x]):
        print("  why %-28s %d" % (k, why[k]))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("files", nargs="*")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--level", default="O1")
    ap.add_argument("-I", dest="includes", action="append", default=[])
    ap.add_argument("-D", dest="defines", action="append", default=[])
    a = ap.parse_args()
    mcc = os.path.join(a.build, "mcc")
    if not os.path.exists(mcc):
        mcc = os.path.join(a.build, "mcc.exe")
    if not os.path.exists(mcc):
        print("SKIP: thread-census: no mcc at %s" % a.build)
        return 77
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if a.selftest:
        return selftest(mcc, root)
    if not a.files:
        print("thread-census: nothing to sweep")
        return 2
    return sweep(mcc, a.files, a.level, a.includes, a.defines)


if __name__ == "__main__":
    sys.exit(main())
