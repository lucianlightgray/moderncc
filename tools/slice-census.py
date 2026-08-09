#!/usr/bin/env python3
"""Census of maximal call-free slices in the RIR production arena.

A *slice* here is the unit a GPU compute shader could plausibly consume: a
maximal run of consecutive statements, inside one statement list of a function
body, whose subtrees contain no `AST_Invoke` the arena cannot see through.
Everything about that definition is load-bearing:

  maximal      a slice cannot be grown by absorbing an adjacent sibling
               statement (the neighbour has a call) nor by moving up to the
               parent statement list (the parent statement has a call).  When a
               whole loop is call-free the slice is the loop, nest and all --
               loops are not split out, they are ordinary statements.
  statements   not expressions.  A call-free *fragment* of a call-containing
               expression (`a[i] + h(s,i)` minus the `h`) is not a slice; that
               is what the pre-existing expression-window machinery in
               src/mccast.c (ast_slice_ident_hash / ast_slice_win_root_ok /
               -fopt-slice) already enumerates, and those windows are 3..64
               nodes of pure integer arithmetic.
  see through  two partitions are reported side by side.
                 t=0  every AST_Invoke is a boundary.
                 t=1  an AST_Invoke whose callee Sym is in the graft-inline
                      pool with graftable=1 is *transparent*: at that -O level
                      the arena replays the callee's body in place, so the
                      call is not a real boundary.  t=1 minus t=0 is exactly
                      the merging effect of inlining.
               Indirect calls (callee is not a Ref to a VT_FUNC Sym) can never
               be transparent; neither can direct calls to symbols with no body
               in this TU.  Those are the "truly anonymous" invokes.

Byte extents are exact, not modelled: src/mccast.c brackets each statement's
replay in ast_replay_bb with the code index `ind`, so every top-level statement
of a slice carries the byte count it emitted during the faithfulness replay.
`attr` vs `bytes` on the per-function line is the reconciliation: attributed
statement bytes against the parser's body length.

Compiler side is the `MCC_SLICE_CENSUS=<path|->` env var, which appends

    [slice]    fn= t= id= stmts= nodes= bytes= depth= loops= kinds=
    [slice-fn] fn= nodes= bytes= attr= stmts= faithful= loops= inv_*= ns/nb/nn=

one `[slice]` per slice and one `[slice-fn]` per modelled body.  Bodies the
arena never modelled emit nothing, so `fn_n` here is the modelled population,
not every function compiled.

Usage:
  tools/slice-census.py <build-dir> [--levels O0,O1,O2,O3]
                        [--corpus self|exec|wide] [--sources ...]
                        [--top N] [--json FILE] [--jobs N] [--opt-in]
"""
import argparse, json, os, shlex, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

KINDS = [
    ("store", 0), ("load", 1), ("binary", 2), ("unary", 3), ("convert", 4),
    ("literal", 5), ("ref", 6), ("jump", 7), ("return", 8), ("storeval", 9),
    ("poison", 10), ("bb", 11), ("if", 12), ("while", 13), ("for", 14),
    ("do", 15), ("switch", 16), ("ternary", 17), ("invoke", 18), ("asm", 19),
    ("float", 20),
]
LOOPY = (1 << 13) | (1 << 14) | (1 << 15)
BUCKETS = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]


def find_mcc(bdir):
    mcc = os.path.join(bdir, "mcc")
    if not os.access(mcc, os.X_OK) and os.access(mcc + ".exe", os.X_OK):
        mcc += ".exe"
    if not os.access(mcc, os.X_OK):
        sys.exit("slice-census: no mcc at " + mcc)
    return mcc


def self_flags(bdir):
    cdb = os.path.join(bdir, "compile_commands.json")
    if not os.path.exists(cdb):
        return []
    cc = json.load(open(cdb))
    rec = [x for x in cc if x["file"].endswith("/mcc.c")]
    if not rec:
        return []
    return [a for a in shlex.split(rec[0]["command"])[1:]
            if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]


def bucket(n):
    for i, b in enumerate(BUCKETS):
        if n <= b:
            return i
    return len(BUCKETS)


def bucket_names():
    out, lo = [], 1
    for b in BUCKETS:
        out.append(str(lo) if lo == b else "%d-%d" % (lo, b))
        lo = b + 1
    out.append("%d+" % lo)
    return out


def new_part():
    return {"n": 0, "nodes": 0, "bytes": 0, "stmts": 0,
            "hist_nodes": [0] * (len(BUCKETS) + 1),
            "hist_bytes": [0] * (len(BUCKETS) + 1),
            "bytes_by_nodebucket": [0] * (len(BUCKETS) + 1),
            "depth": {}, "loops": 0, "loop_nodes": 0, "loop_bytes": 0,
            "kinds": {k: 0 for k, _ in KINDS}, "top": []}


def new_acc():
    return {"fn_n": 0, "fn_faithful": 0, "body_bytes": 0, "attr_bytes": 0,
            "nodes": 0, "ovf": 0, "src_n": 0, "src_fail": 0,
            "inv_ind": 0, "inv_ext": 0, "inv_ret": 0, "inv_graft": 0,
            "fn_callfree": 0, "fn_callfree_bytes": 0,
            "part": [new_part(), new_part()]}


def kv(fields):
    d = {}
    for f in fields:
        if "=" in f:
            k, v = f.split("=", 1)
            d[k] = v
    return d


def verify_fn(m, seen):
    """Invariants a per-body record must satisfy, whatever the corpus.

    Transparency can only grow coverage, never shrink it -- it may also raise
    the slice *count*, because a body that was wholly call-covered at t=0 can
    acquire its first slice at t=1, so only nodes and bytes are monotone.  A
    body with no invoke at all has to come out as exactly one slice covering
    every statement and every attributed byte.

    A peephole that rewinds `ind` can retract bytes an earlier statement
    already emitted, so per-statement attribution never loses a byte but can
    double-count a retracted one.  Measured drift on src/mcc.c is 0.005% of
    body bytes at -O1..-O3 and 0.05% at -O0, in four bodies; hence the slack.
    """
    out = []
    fn = m["fn"]
    nodes, byts, attr = int(m["nodes"]), int(m["bytes"]), int(m["attr"])
    ns0, nb0, nn0 = int(m["ns0"]), int(m["nb0"]), int(m["nn0"])
    ns1, nb1, nn1 = int(m["ns1"]), int(m["nb1"]), int(m["nn1"])
    ninv = sum(int(m[k]) for k in
               ("inv_ind", "inv_ext", "inv_ret", "inv_graft"))
    if nn0 > nodes or nn1 > nodes:
        out.append("%s: slice nodes %d/%d exceed arena nodes %d"
                   % (fn, nn0, nn1, nodes))
    slack = max(32, byts // 50)
    if nb0 > byts + slack or nb1 > byts + slack:
        out.append("%s: slice bytes %d/%d exceed body bytes %d by more than %d"
                   % (fn, nb0, nb1, byts, slack))
    if nn1 < nn0 or nb1 < nb0:
        out.append("%s: t=1 covers less than t=0 (%d/%d/%d vs %d/%d/%d)"
                   % (fn, ns1, nn1, nb1, ns0, nn0, nb0))
    if ninv == 0 and int(m["ovf"]) == 0 and int(m["stmts"]) > 0:
        if ns0 != 1:
            out.append("%s: call-free body split into %d slices" % (fn, ns0))
        if nn0 != nodes - 1:
            out.append("%s: call-free body covers %d of %d-1 nodes"
                       % (fn, nn0, nodes))
        if nb0 != attr:
            out.append("%s: call-free body covers %d of %d attributed bytes"
                       % (fn, nb0, attr))
    if int(m["inv_graft"]) == 0 and (ns1, nn1, nb1) != (ns0, nn0, nb0):
        out.append("%s: no graftable invoke but t=1 differs from t=0" % fn)
    seen[fn] = (ns0, nn0, nb0)
    return out


def absorb(acc, text, top_n, verify=None, seen=None):
    for line in text.splitlines():
        f = line.split()
        if not f:
            continue
        if f[0] == "[slice]":
            m = kv(f[1:])
            t = int(m["t"])
            p = acc["part"][t]
            nodes, byts = int(m["nodes"]), int(m["bytes"])
            kinds = int(m["kinds"], 16)
            p["n"] += 1
            p["nodes"] += nodes
            p["stmts"] += int(m["stmts"])
            if byts >= 0:
                p["bytes"] += byts
            bn = bucket(nodes)
            p["hist_nodes"][bn] += 1
            p["hist_bytes"][bucket(max(byts, 0))] += 1
            p["bytes_by_nodebucket"][bn] += max(byts, 0)
            d = int(m["depth"])
            p["depth"][d] = p["depth"].get(d, 0) + 1
            if int(m["loops"]):
                p["loops"] += 1
                p["loop_nodes"] += nodes
                p["loop_bytes"] += max(byts, 0)
            for name, bit in KINDS:
                if kinds & (1 << bit):
                    p["kinds"][name] += 1
            if top_n:
                p["top"].append((nodes, max(byts, 0), int(m["loops"]), d,
                                 m["fn"]))
                if len(p["top"]) > 4 * top_n:
                    p["top"].sort(reverse=True)
                    del p["top"][top_n:]
        elif f[0] == "[slice-fn]":
            m = kv(f[1:])
            acc["fn_n"] += 1
            acc["fn_faithful"] += int(m["faithful"])
            acc["body_bytes"] += int(m["bytes"])
            acc["attr_bytes"] += int(m["attr"])
            acc["nodes"] += int(m["nodes"])
            acc["ovf"] += int(m["ovf"])
            for k in ("inv_ind", "inv_ext", "inv_ret", "inv_graft"):
                acc[k] += int(m[k])
            if int(m["ns0"]) == 1 and int(m["nb0"]) == int(m["bytes"]):
                acc["fn_callfree"] += 1
                acc["fn_callfree_bytes"] += int(m["bytes"])
            if verify is not None:
                verify.extend(verify_fn(m, seen))


def run_one(mcc, flags, src, opt, tmpdir, idx):
    cen = os.path.join(tmpdir, "c%d.txt" % idx)
    out_o = os.path.join(tmpdir, "o%d.o" % idx)
    env = dict(os.environ)
    for k in ("MCC_TEST_OPT", "MCC_RIR_PROD", "MCC_RIR_PROD_OUT",
              "MCC_REPLAY_IR"):
        env.pop(k, None)
    env["MCC_SLICE_CENSUS"] = cen
    if opt == "O0":
        env["MCC_FORCE_REPLAY"] = "1"
    else:
        env.pop("MCC_FORCE_REPLAY", None)
    cmd = [mcc] + flags + ["-" + opt, "-c", src, "-o", out_o]
    p = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT, env=env)
    txt = ""
    if os.path.exists(cen):
        txt = open(cen, errors="replace").read()
        os.unlink(cen)
    if os.path.exists(out_o):
        os.unlink(out_o)
    return p.returncode, txt


def census(mcc, flags, sources, opt, top_n, jobs, verify=None):
    acc = new_acc()
    with tempfile.TemporaryDirectory() as td:
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            futs = [ex.submit(run_one, mcc, flags, s, opt, td, i)
                    for i, s in enumerate(sources)]
            for fu in futs:
                rc, txt = fu.result()
                acc["src_n"] += 1
                if rc != 0:
                    acc["src_fail"] += 1
                absorb(acc, txt, top_n, verify, {})
    for p in acc["part"]:
        p["top"].sort(reverse=True)
        del p["top"][top_n:]
    return acc


def pct(a, b):
    return 100.0 * a / b if b else 0.0


def report(level, acc, top_n):
    names = bucket_names()
    print("=== -%s  sources=%d (failed %d)  modelled bodies=%d "
          "(faithful %d)" % (level, acc["src_n"], acc["src_fail"],
                             acc["fn_n"], acc["fn_faithful"]))
    print("    body bytes %d   statement-attributed %d (%.3f%%)   arena nodes %d"
          % (acc["body_bytes"], acc["attr_bytes"],
             pct(acc["attr_bytes"], acc["body_bytes"]), acc["nodes"]))
    inv_tot = sum(acc[k] for k in ("inv_ind", "inv_ext", "inv_ret", "inv_graft"))
    print("    invokes %d = indirect %d + opaque-direct %d + retained %d + "
          "graftable %d  (anonymous %d = %.1f%%)"
          % (inv_tot, acc["inv_ind"], acc["inv_ext"], acc["inv_ret"],
             acc["inv_graft"], acc["inv_ind"] + acc["inv_ext"],
             pct(acc["inv_ind"] + acc["inv_ext"], inv_tot)))
    print("    wholly call-free bodies %d/%d (%.1f%%) = %d B (%.2f%% of body "
          "bytes)" % (acc["fn_callfree"], acc["fn_n"],
                      pct(acc["fn_callfree"], acc["fn_n"]),
                      acc["fn_callfree_bytes"],
                      pct(acc["fn_callfree_bytes"], acc["body_bytes"])))
    for t in (0, 1):
        p = acc["part"][t]
        tag = "t=0 every invoke opaque " if t == 0 else "t=1 graftable inlined"
        print("  -- %s : %d slices, %d nodes (%.2f%% of arena), %d B "
              "(%.2f%% of body bytes)"
              % (tag, p["n"], p["nodes"], pct(p["nodes"], acc["nodes"]),
                 p["bytes"], pct(p["bytes"], acc["body_bytes"])))
        print("     nodes/slice histogram (count | bytes in bucket):")
        for i, nm in enumerate(names):
            if not p["hist_nodes"][i]:
                continue
            print("       %-9s %7d  %5.1f%%   %9d B  %5.1f%%"
                  % (nm, p["hist_nodes"][i], pct(p["hist_nodes"][i], p["n"]),
                     p["bytes_by_nodebucket"][i],
                     pct(p["bytes_by_nodebucket"][i], p["bytes"])))
        print("     bytes/slice histogram:")
        for i, nm in enumerate(names):
            if p["hist_bytes"][i]:
                print("       %-9s %7d  %5.1f%%"
                      % (nm, p["hist_bytes"][i], pct(p["hist_bytes"][i], p["n"])))
        print("     loop-nest depth of slice:  " + "  ".join(
            "d%d=%d" % (d, p["depth"][d]) for d in sorted(p["depth"])))
        print("     slices containing a loop: %d (%.2f%%), %d nodes, %d B "
              "(%.2f%% of body bytes)"
              % (p["loops"], pct(p["loops"], p["n"]), p["loop_nodes"],
                 p["loop_bytes"], pct(p["loop_bytes"], acc["body_bytes"])))
        ks = sorted(p["kinds"].items(), key=lambda x: -x[1])
        print("     kinds present in slices: " + "  ".join(
            "%s=%.0f%%" % (k, pct(v, p["n"])) for k, v in ks if v))
        if top_n and p["top"]:
            print("     largest %d slices (nodes, bytes, loops, depth, fn):"
                  % min(top_n, len(p["top"])))
            for nodes, byts, loops, d, fn in p["top"][:top_n]:
                print("       %6d %6d %3d %3d  %s" % (nodes, byts, loops, d, fn))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir")
    ap.add_argument("--levels", default="O0,O1,O2,O3")
    ap.add_argument("--corpus", default="self",
                    choices=["self", "exec", "wide"])
    ap.add_argument("--sources", nargs="*", default=None)
    ap.add_argument("--top", type=int, default=10)
    ap.add_argument("--json", default=None)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--opt-in", action="store_true")
    ap.add_argument("--min-slices", type=int, default=0)
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    if a.opt_in and not os.environ.get("MCC_SLICE_CENSUS_RUN"):
        print("slice-census: set MCC_SLICE_CENSUS_RUN=1 to run this census "
              "(ctest -L census)")
        return 77

    bdir = a.build_dir if os.path.isabs(a.build_dir) \
        else os.path.join(ROOT, a.build_dir)
    mcc = find_mcc(bdir)
    flags = self_flags(bdir)

    if a.sources:
        sources = a.sources
    elif a.corpus == "self":
        sources = [os.path.join(ROOT, "src", "mcc.c")]
    else:
        sources = [] if a.corpus == "exec" else [os.path.join(ROOT, "src", "mcc.c")]
        for d in (("tests/exec",) if a.corpus == "exec" else
                  ("tests/exec", "tests/behavior", "tests/ast", "examples")):
            p = os.path.join(ROOT, d)
            if not os.path.isdir(p):
                continue
            for dp, _, fns in os.walk(p):
                for fn in sorted(fns):
                    if fn.endswith(".c"):
                        sources.append(os.path.join(dp, fn))

    out = {}
    bad = []
    for level in a.levels.split(","):
        level = level.strip()
        if not level:
            continue
        errs = [] if a.verify else None
        acc = census(mcc, flags, sources, level, a.top, a.jobs, errs)
        if not a.quiet:
            report(level, acc, a.top)
        if errs:
            bad += ["-%s %s" % (level, e) for e in errs]
        out[level] = acc

    if a.json:
        json.dump(out, open(a.json, "w"), indent=1, sort_keys=True)

    for level, acc in out.items():
        if acc["src_fail"]:
            bad.append("-%s: %d of %d sources did not compile, so every share "
                       "below is over the survivors. The count reaches stdout "
                       "only via report(), which --quiet suppresses, and "
                       "nothing gated it: 11 of 12 sources could drop out and "
                       "the cell would still pass on the twelfth."
                       % (level, acc["src_fail"], acc["src_n"]))
        if not acc["fn_n"]:
            bad.append("-%s: zero bodies were modelled, so every percentage is "
                       "over an empty denominator and the OK line below would "
                       "read `0 slices (0.0%% of 0 body bytes)` as a result"
                       % level)
        if a.min_slices and acc["part"][0]["n"] < a.min_slices:
            bad.append("-%s: enumerated %d slices, expected >= %d"
                       % (level, acc["part"][0]["n"], a.min_slices))
    if bad:
        for b in bad[:40]:
            print("slice-census: " + b)
        if len(bad) > 40:
            print("slice-census: ... and %d more" % (len(bad) - 40))
        return 1
    if a.quiet:
        for level, acc in out.items():
            p = acc["part"][0]
            print("slice-census -%s: %d bodies, %d slices, %d B in slices "
                  "(%.1f%% of %d body bytes)  OK"
                  % (level, acc["fn_n"], p["n"], p["bytes"],
                     pct(p["bytes"], acc["body_bytes"]), acc["body_bytes"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
