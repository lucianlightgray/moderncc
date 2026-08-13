#!/usr/bin/env python3
"""N18: the -O0 --embed-jit arm is the least faithful one, and nothing watched it.

Three compiles and two subtractions.  That is the whole measurement, and it is
the point: docs/TODO.md priced this row against tools/emit-map.py, which needs
MCC_CONFIG_TRACE=ON, runs for up to 7200 s and is opt-in -- so no ordinary
build watched the arm at all.  The same table reproduces from MCC_INV=1 on the
build everyone already has, in about a second per target, because the
faithfulness verdict is counted by the compiler at the site rather than
recovered from a trace.

What it measures, per target:

  ast.body      bodies that reached a faithfulness verdict in ast_func_end
  ast.faithful  of those, the ones whose replay emitted the parser's bytes
  unfaithful%   the complement, which is the number this row is about
  gap           unfaithful% at -O0 --embed-jit MINUS unfaithful% at -O1
  jit_gap       unfaithful% at -O1 --embed-jit MINUS unfaithful% at -O1

THE THIRD ARM IS THE WHOLE LESSON OF THIS FILE, and it was missing until
2026-08-13.  ARMS was (-O1, -O0 --embed-jit): two factors moving at once, so a
non-zero gap could be read as either, and this file's own name and docstring
read it as the JIT for as long as they existed.  It is not the JIT.  With -O1
held fixed, turning --embed-jit ON moves faithfulness by EXACTLY ZERO on both
targets; the entire published gap was the -O level.  The control costs about a
second and it is the difference between naming a defect and measuring one.

The gap WAS +1.20 and +1.36 points on x86_64 (2026-08-11), +1.29 and +1.85 on
arm64/macOS (2026-08-12).  Root cause: storeval-rot and storeval-calllast are
replay-shape recognizers, not optimizations, and were MCC_OPTD_LEVEL(1) -- so
the -O0 replay arm (reachable in shipping builds only through --embed-jit, via
ast_replay_env) ran without them.  Both are MCC_OPTD_ALWAYS as of that date and
the gap is -0.31 and +0.04 here.  arm64/macOS has not re-taken it.

Since 3e0f1e8d made --embed-jit the only gate on baking, this is the arm a
`-g --embed-jit` build takes, so its unfaithful bodies are the ones silently
excluded from dispatch in a debug configuration.

Also banked, because N6.8 established the published bake rate was the wrong
counter: jit.baked counts CALLS to a single-slot stash that the callee may
refuse and the next body overwrites, while jit.embed counts appends to
mccjit_embed_fns, the list that is actually baked into the binary.  Self-host
reads 1577 against 1278 -- the 49% bake rate this file published was 39.9%.

Exit 0 clean, 1 on a banked figure moving past tolerance, 2 on a usage or
measurement problem, 77 when the build cannot supply the counters.
"""

import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from importlib import import_module

emit_map = import_module("emit-map")

ARMS = (("-O1", False), ("-O1", True), ("-O0", True))
TOL = 0.35


def die(msg):
    sys.stderr.write("inv-faithful: %s\n" % msg)
    raise SystemExit(2)


def measure(mcc, root, bdir, target, opt, embed_jit):
    cmd = emit_map.build_cmd(mcc, root, bdir, target, opt, embed_jit, [])
    env = dict(os.environ, MCC_INV="1")
    env.pop("MCC_LOG", None)
    p = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                       env=env)
    if p.returncode != 0:
        die("compile failed (rc=%d) for %s %s%s:\n%s"
            % (p.returncode, target, opt, " --embed-jit" if embed_jit else "",
               p.stderr.decode("utf8", "replace")[-2000:]))
    inv = {}
    for ln in p.stderr.decode("utf8", "replace").splitlines():
        if not ln.startswith("[invcount]"):
            continue
        for kv in ln.split()[1:]:
            k, eq, v = kv.partition("=")
            if not eq:
                die("unparsable [invcount] token %r" % kv)
            inv[k] = int(v)
    return inv


def arm_figures(inv):
    body = inv.get("ast.body", 0)
    if not body:
        return None
    unfaithful = body - inv.get("ast.faithful", 0)
    return {"verdicted": body,
            "faithful": inv.get("ast.faithful", 0),
            "unfaithful": unfaithful,
            "unfaithful_pct": round(100.0 * unfaithful / body, 2),
            "bake_attempts": inv.get("jit.baked", 0),
            "baked": inv.get("jit.embed", 0),
            "baked_pct": round(100.0 * inv.get("jit.embed", 0) / body, 2)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bdir")
    ap.add_argument("--root", default=None)
    ap.add_argument("--target", action="append", default=[])
    ap.add_argument("--bank", default=None)
    ap.add_argument("--update-bank", action="store_true")
    ap.add_argument("--known-positive", action="store_true",
                    help="perturb the measured gap and require the bank "
                         "comparison to catch it")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    root = a.root or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bdir = a.bdir if os.path.isabs(a.bdir) else os.path.join(root, a.bdir)
    mcc = os.path.join(bdir, "mcc")
    if not os.path.exists(mcc):
        print("SKIP: no mcc in %s" % bdir)
        return 77
    arch = emit_map.target_arch(mcc)
    if not arch:
        print("SKIP: could not determine the target arch of %s" % mcc)
        return 77
    targets = a.target or ["full_language.c", "selfhost"]

    out, report = {}, []
    for t in targets:
        arms = {}
        for opt, jit in ARMS:
            name = opt + (" --embed-jit" if jit else "")
            inv = measure(mcc, root, bdir, t, opt, jit)
            if inv.get("inv.dropped"):
                die("the inventory table overflowed MCC_INV_MAX and dropped %d "
                    "increment(s) on %s %s; every key it lost reads as a "
                    "genuine zero" % (inv["inv.dropped"], t, name))
            f = arm_figures(inv)
            if f is None:
                print("SKIP: no ast.body counter from %s %s - build lacks "
                      "MCC_INV instrumentation" % (t, name))
                return 77
            arms[name] = f
        base = arms["-O1"]
        jitarm = arms["-O0 --embed-jit"]
        ctrl = arms["-O1 --embed-jit"]
        gap = round(jitarm["unfaithful_pct"] - base["unfaithful_pct"], 2)
        jit_gap = round(ctrl["unfaithful_pct"] - base["unfaithful_pct"], 2)
        out["%s|%s" % (arch, t)] = {
            "unfaithful_pct_O1": base["unfaithful_pct"],
            "unfaithful_pct_O1_embedjit": ctrl["unfaithful_pct"],
            "unfaithful_pct_embedjit": jitarm["unfaithful_pct"],
            "embedjit_gap_pt": gap,
            "jit_gap_pt": jit_gap,
            "baked_pct": jitarm["baked_pct"]}
        report.append((t, arms, gap, jit_gap))

    if a.json:
        print(json.dumps(out, indent=1, sort_keys=True))
    else:
        for t, arms, gap, jit_gap in report:
            print("inv-faithful %s [%s]" % (t, arch))
            for name in ("-O1", "-O1 --embed-jit", "-O0 --embed-jit"):
                f = arms[name]
                print("  %-16s %5d verdicted  %5d unfaithful = %5.2f%%"
                      % (name, f["verdicted"], f["unfaithful"],
                         f["unfaithful_pct"]))
            j = arms["-O0 --embed-jit"]
            print("  %-16s %+5.2f points, and it is the -O LEVEL that moves it"
                  % ("GAP", gap))
            print("  %-16s %+5.2f points -- the JIT axis alone, -O1 held fixed"
                  % ("JIT GAP", jit_gap))
            print("  %-16s %d of %d bodies (%.2f%%) reached mccjit_embed_fns; "
                  "%d stash attempts"
                  % ("baked", j["baked"], j["verdicted"], j["baked_pct"],
                     j["bake_attempts"]))

    if a.known_positive:
        for cell in out.values():
            cell["embedjit_gap_pt"] = round(cell["embedjit_gap_pt"] + 5.0, 2)
            cell["jit_gap_pt"] = round(cell["jit_gap_pt"] + 5.0, 2)

    if not a.bank:
        return 0
    bank = {}
    if os.path.exists(a.bank):
        with open(a.bank, encoding="utf8") as f:
            bank = json.load(f)
    if a.update_bank:
        bank.update(out)
        os.makedirs(os.path.dirname(a.bank) or ".", exist_ok=True)
        with open(a.bank, "w", encoding="utf8") as f:
            json.dump(bank, f, indent=1, sort_keys=True)
            f.write("\n")
        print("banked %s" % ", ".join(sorted(out)))
        return 0
    bad, seen = [], 0
    for key, cell in sorted(out.items()):
        if key not in bank:
            print("SKIP: no banked figures for %s (use --update-bank)" % key)
            continue
        seen += 1
        for k, got in sorted(cell.items()):
            want = bank[key].get(k)
            if want is None:
                continue
            if abs(got - want) > TOL:
                bad.append("%s %s: banked %s, now %s (tolerance %s)"
                           % (key, k, want, got, TOL))
    if not seen:
        return 77
    if a.known_positive:
        if bad:
            print("known-positive: the bank caught the injected +5.00pt gap "
                  "(%d finding(s))" % len(bad))
            return 0
        print("FAIL: known-positive: a +5.00pt gap on every target was banked "
              "as unchanged, so this cell cannot go red")
        return 1
    if bad:
        for b in bad:
            print("FAIL: %s" % b)
        return 1
    print("inv-faithful bank OK for %s" % ", ".join(sorted(out)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
