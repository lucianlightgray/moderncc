#!/usr/bin/env python3
"""S5' -- the dynamic per-loop trip-count census over a self-compile.

`docs/PLAN.md` calls the iteration distribution "the single measurement that
decides whether the project has a subject", and nothing in this tree computes a
static trip count.  So the measurement is dynamic: mcc compiles its own source
with `-floop-census`, which

  * gives every `while` / `for` / `do` in the translation unit an id,
  * writes a static `[loop]` record for it to `MCC_LOOP_CENSUS_MAP`,
  * allocates a hidden 8-byte frame slot, zeroes it at loop entry, increments it
    at the top of the body, and calls `__mcc_loop_census(id, trips)` at the loop
    exit label -- which is *after* `gsym(bsym)`, so a `break` is covered too.

`return`, `goto` and `longjmp` out of a loop escape that exit label.  Those
entries are counted by `__mcc_loop_census_enter` and never closed, so they show
up as `lost=` and are reported, not hidden.  The mirror case -- a `goto` *into* a
loop body, which reaches the exit without ever running the entry code -- shows up
as `stray=`, and the trip counts of a loop with a nonzero `stray` are not
trustworthy, so those loops are named in the report.

The instrumented compiler then compiles the same source again, and its
`runtime/lib/loopcensus.c` destructor dumps one `[trip]` line per loop to
`MCC_LOOP_CENSUS`.

The statistic that matters is NOT mean trips.  A slice is worth moving to the
device only if its trip count clears the break-even lane count for its own body
size (docs/TODO.md):

    body nodes    3     7    15    31    63   127
    needs trips  322   108    48    24    23     8

so the number reported is the *iteration-weighted* fraction: of all iterations
executed anywhere in the compile, what share happened in a loop entry whose trip
count met the threshold for that loop's own size bucket.  A loop entered a
million times for 2 trips contributes two million iterations to the denominator
and nothing to the numerator.

Loop body size in AST nodes is not available at parse time -- the arena is built
from the RIR recording after the body is over -- so the compiler reports the two
sizes it does know exactly: emitted code `bytes` (instrumentation excluded) and
`toks` (preprocessed tokens).  `bytes` is converted to nodes with a calibration
measured on the same translation unit in the same run: MCC_SLICE_CENSUS emits
`nodes=` and `bytes=` for every call-free slice, and their ratio is the
bytes-per-node constant printed in the output.  Because that constant is a
measurement and not a model, the report also prints the fraction at each of the
six fixed thresholds, so the conclusion can be read without trusting it.

Usage:
  tools/loop-census.py <build-dir> --selftest
      the positive control: tests/loopcensus/known_trips.c has trip counts that
      are known by construction, and known_trips.expect holds them.  Checked at
      -O0/-O1/-O2/-O3, with a negative control (no flag -> no data) and a
      perturbation (move a loop bound -> the histogram moves).

  tools/loop-census.py <build-dir> [--source src/mcc.c] [--top N]
                       [--levels O2] [--json FILE] [--keep DIR] [--opt-in]
      the self-compile census.  --opt-in makes it exit 77 unless
      MCC_LOOP_CENSUS_RUN=1, because it self-compiles three times.
"""
import argparse, json, os, re, shlex, subprocess, sys, tempfile
from importlib import machinery, util as importlib_util

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

_ldr = machinery.SourceFileLoader(
    "slice_census", os.path.join(ROOT, "tools", "slice-census.py"))
_spec = importlib_util.spec_from_loader(_ldr.name, _ldr)
slice_census = importlib_util.module_from_spec(_spec)
_ldr.exec_module(slice_census)

bucket = slice_census.bucket
bucket_names = slice_census.bucket_names

BREAKEVEN = [(3, 322), (7, 108), (15, 48), (31, 24), (63, 23), (127, 8)]
THRESHOLDS = [8, 23, 24, 48, 108, 322]


def breakeven_trips(nodes):
    for n, t in BREAKEVEN:
        if nodes <= n:
            return t
    return BREAKEVEN[-1][1]


def find_mcc(bdir):
    mcc = os.path.join(bdir, "mcc")
    if not os.access(mcc, os.X_OK) and os.access(mcc + ".exe", os.X_OK):
        mcc += ".exe"
    return mcc


def self_flags(bdir, src):
    p = os.path.join(bdir, "compile_commands.json")
    if not os.path.exists(p):
        return None
    cc = json.load(open(p))
    base = os.path.basename(src)
    rec = [x for x in cc if x["file"].endswith("/" + base)]
    if not rec:
        return None
    cmd = rec[0]["command"]
    if os.name == "nt":
        cmd = re.sub(r'\\(?!")', r'\\\\', cmd)
    return [a for a in shlex.split(cmd)[1:]
            if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]


def link_libs(bdir, pe):
    p = os.path.join(bdir, "selfhost-link-libs.txt")
    if not os.path.exists(p):
        return [] if pe else ["-lm", "-ldl"]
    with open(p) as f:
        return [ln.strip() for ln in f if ln.strip()]


def parse_map(txt):
    loops = {}
    for ln in txt.splitlines():
        f = ln.split()
        if not f or f[0] != "[loop]":
            continue
        m = {}
        for kv in f[1:]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                m[k] = v
        try:
            i = int(m["id"])
        except (KeyError, ValueError):
            continue
        loops[i] = {
            "fn": m.get("fn", "?"),
            "file": m.get("file", "?"),
            "kind": m.get("kind", "?"),
            "depth": int(m.get("depth", 0)),
            "bytes": int(m.get("bytes", 0)),
            "toks": int(m.get("toks", 0)),
        }
    return loops


def parse_trips(txt):
    trips, tot = {}, None
    for ln in txt.splitlines():
        f = ln.split()
        if not f:
            continue
        m = {}
        for kv in f[1:]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                m[k] = v
        if f[0] == "[trip]":
            i = int(m["id"])
            h = [int(x) for x in m["h"].split(",")]
            ge = {int(p.split(":")[0]): int(p.split(":")[1])
                  for p in m["ge"].split(",")}
            gew = {int(p.split(":")[0]): int(p.split(":")[1])
                   for p in m["gew"].split(",")}
            trips[i] = {
                "entries": int(m["entries"]), "exits": int(m["exits"]),
                "lost": int(m["lost"]), "stray": int(m.get("stray", 0)),
                "iters": int(m["iters"]), "zero": int(m["zero"]),
                "max": int(m.get("max", 0)), "h": h, "ge": ge, "gew": gew,
            }
        elif f[0] == "[trip-tot]":
            tot = {k: int(v) for k, v in m.items()}
    return trips, tot


def calibrate_bytes_per_node(mcc, flags, src, opt, work):
    """Measured bytes-per-AST-node on this exact TU, from MCC_SLICE_CENSUS."""
    cen = os.path.join(work, "slice.txt")
    env = dict(os.environ)
    for k in ("MCC_TEST_OPT", "MCC_RIR_PROD", "MCC_RIR_PROD_OUT",
              "MCC_REPLAY_IR", "MCC_LOOP_CENSUS", "MCC_LOOP_CENSUS_MAP"):
        env.pop(k, None)
    env["MCC_SLICE_CENSUS"] = cen
    subprocess.run([mcc] + flags + ["-" + opt, "-c", src, "-o",
                    os.path.join(work, "cal.o")],
                   cwd=ROOT, env=env, capture_output=True, text=True)
    if not os.path.exists(cen):
        return None, 0
    ratios = []
    for ln in open(cen, errors="replace"):
        f = ln.split()
        if not f or f[0] != "[slice]":
            continue
        m = {}
        for kv in f[1:]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                m[k] = v
        n, b = int(m.get("nodes", 0)), int(m.get("bytes", -1))
        if n > 0 and b > 0:
            ratios.append(b / n)
    if not ratios:
        return None, 0
    ratios.sort()
    return ratios[len(ratios) // 2], len(ratios)


def run(bdir, src, opt, top, keep, want_json):
    mcc = find_mcc(bdir)
    if not os.access(mcc, os.X_OK):
        print("loop-census: no mcc at %s" % mcc)
        return 77
    flags = self_flags(bdir, src)
    if flags is None:
        print("loop-census: no compile_commands.json entry for %s" % src)
        return 77
    pe = mcc.endswith(".exe")
    blob = os.path.join(bdir, "CMakeFiles", "mcc.dir", "mccrt_blob.c.o")
    link_objs, link_flags = [], []
    if os.path.exists(blob):
        link_objs.append(blob)
    else:
        sidecar = next((p for p in (os.path.join(bdir, "libmccrt.a"),
                                    os.path.join(bdir, "lib", "libmccrt.a"))
                        if os.path.exists(p)), None)
        if not sidecar:
            print("loop-census: no runtime blob at %s" % blob)
            return 77
        link_flags += ["-B", os.path.dirname(sidecar)]
    if any(a.startswith("-DMCC_EMBED_JIT_BLOB") for a in flags):
        jbase = os.path.join(bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c")
        jit = next((jbase + e for e in (".o", ".obj")
                    if os.path.exists(jbase + e)), None)
        if not jit:
            print("loop-census: no JIT blob at %s.o" % jbase)
            return 77
        link_objs.append(jit)
    win32_pre = ["-B", os.path.join(ROOT, "runtime/win32")] if pe else []
    libs = link_libs(bdir, pe)
    exe = ".exe" if pe else ""

    ctx = (tempfile.TemporaryDirectory() if not keep
           else _KeepDir(keep))
    with ctx as work:
        mapf = os.path.join(work, "map.txt")
        lcf = os.path.join(work, "lc.txt")
        obj = os.path.join(work, "mcc-lc.o")
        env = dict(os.environ)
        env.pop("MCC_LOOP_CENSUS", None)
        env["MCC_LOOP_CENSUS_MAP"] = mapf
        print("loop-census: compiling %s with -floop-census" % src)
        p = subprocess.run([mcc, *flags, "-" + opt, "-floop-census", "-c",
                            os.path.join(ROOT, src), "-o", obj],
                           cwd=ROOT, env=env, capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout + p.stderr)
            print("loop-census: instrumented compile FAILED")
            return 1
        loops = parse_map(open(mapf, errors="replace").read()
                          if os.path.exists(mapf) else "")
        if not loops:
            print("loop-census: no [loop] records emitted")
            return 1
        print("loop-census: %d loops instrumented" % len(loops))

        binp = os.path.join(work, "mcc-lc" + exe)
        print("loop-census: linking the instrumented compiler")
        p = subprocess.run([mcc, *link_flags, *win32_pre, obj, *link_objs,
                            "-o", binp, *libs],
                           cwd=ROOT, capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout + p.stderr)
            print("loop-census: link FAILED")
            return 1

        print("loop-census: self-compiling under the instrumented compiler")
        env2 = dict(os.environ)
        env2.pop("MCC_LOOP_CENSUS_MAP", None)
        env2["MCC_LOOP_CENSUS"] = lcf
        devnull = os.path.join(work, "out.o")
        rtinc = "-I" + os.path.join(ROOT, "runtime/include")
        p = subprocess.run([binp, rtinc, *flags, "-" + opt, "-c",
                            os.path.join(ROOT, src), "-o", devnull],
                           cwd=ROOT, env=env2, capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout[-4000:] + p.stderr[-4000:])
            print("loop-census: instrumented self-compile FAILED")
            return 1
        if not os.path.exists(lcf):
            print("loop-census: no dump at %s" % lcf)
            return 1
        trips, tot = parse_trips(open(lcf, errors="replace").read())
        bpn, ncal = calibrate_bytes_per_node(mcc, flags, os.path.join(ROOT, src),
                                             opt, work)

    return report(loops, trips, tot, bpn, ncal, top, opt, src, want_json)


class _KeepDir:
    def __init__(self, d):
        self.d = d
        os.makedirs(d, exist_ok=True)

    def __enter__(self):
        return self.d

    def __exit__(self, *a):
        return False


def report(loops, trips, tot, bpn, ncal, top, opt, src, want_json):
    names = bucket_names()
    hist = [0] * len(names)
    tot_iters = tot_entries = tot_lost = tot_zero = tot_stray = 0
    stray_loops = []
    per_fn = {}
    weighted_num = 0
    fixed_num = {t: 0 for t in THRESHOLDS}
    fixed_ent = {t: 0 for t in THRESHOLDS}
    nodes_hist = [0] * len(names)
    unmapped = 0
    rows = []
    for i, tr in trips.items():
        lp = loops.get(i)
        if lp is None:
            unmapped += 1
            continue
        for j, v in enumerate(tr["h"]):
            hist[j] += v
        tot_iters += tr["iters"]
        tot_entries += tr["entries"]
        tot_lost += tr["lost"]
        tot_stray += tr["stray"]
        tot_zero += tr["zero"]
        if tr["stray"]:
            stray_loops.append((tr["stray"], i, lp, tr))
        est = max(1, int(round(lp["bytes"] / bpn))) if bpn else lp["toks"]
        nodes_hist[bucket(est)] += tr["iters"]
        thr = breakeven_trips(est)
        weighted_num += tr["gew"][thr]
        for t in THRESHOLDS:
            fixed_num[t] += tr["gew"][t]
            fixed_ent[t] += tr["ge"][t]
        a = per_fn.setdefault(lp["fn"], {"loops": 0, "entries": 0, "iters": 0,
                                         "lost": 0, "hot": 0})
        a["loops"] += 1
        a["entries"] += tr["entries"]
        a["iters"] += tr["iters"]
        a["lost"] += tr["lost"]
        a["hot"] += tr["gew"][thr]
        rows.append((tr["iters"], i, lp, tr, est, thr))

    print()
    print("=== loop census: %s at -%s ===" % (src, opt))
    print("loops instrumented    %d" % len(loops))
    print("loops entered         %d" % len(rows))
    print("loop entries          %d" % tot_entries)
    print("iterations            %d" % tot_iters)
    print("entries lost (return/goto/longjmp out of the loop)  %d  "
          "(%.3f%% of entries)"
          % (tot_lost, 100.0 * tot_lost / tot_entries if tot_entries else 0.0))
    print("stray exits (goto INTO a loop body, no entry seen)  %d  "
          "(%.3f%% of entries)"
          % (tot_stray, 100.0 * tot_stray / tot_entries if tot_entries else 0.0))
    print("entries with 0 trips  %d" % tot_zero)
    if tot:
        print("runtime totals        entries=%d exits=%d lost=%d stray=%d "
              "iters=%d overflow=%d"
              % (tot["entries"], tot["exits"], tot["lost"], tot.get("stray", 0),
                 tot["iters"], tot["overflow"]))
    if stray_loops:
        stray_loops.sort(reverse=True)
        print("  loops with stray exits (their trip counts are suspect):")
        for s, i, lp, tr in stray_loops[:8]:
            print("    id=%-5d %-24s %-6s stray=%-8d entries=%-9d max=%d"
                  % (i, lp["fn"][:24], lp["kind"], s, tr["entries"], tr["max"]))
    if unmapped:
        print("WARNING: %d [trip] ids had no [loop] record" % unmapped)

    print()
    print("trip-count histogram (loop entries, power-of-two buckets)")
    ent_tot = sum(hist)
    for n, v in zip(names, hist):
        if v:
            print("  %-8s %12d  %6.2f%%" % (n, v, 100.0 * v / ent_tot))
    print()
    print("iterations by loop-body size bucket (estimated AST nodes)")
    for n, v in zip(names, nodes_hist):
        if v:
            print("  %-8s %12d  %6.2f%%" % (n, v, 100.0 * v / tot_iters))

    print()
    if bpn:
        print("bytes-per-node calibration  %.2f  (median over %d census slices)"
              % (bpn, ncal))
    else:
        print("bytes-per-node calibration  UNAVAILABLE -- fell back to toks")
    wf = 100.0 * weighted_num / tot_iters if tot_iters else 0.0
    print("ITERATION-WEIGHTED FRACTION AT EACH LOOP'S OWN BREAK-EVEN   %.2f%%"
          % wf)
    print("  (share of executed iterations in loop entries whose trip count")
    print("   meets the break-even lane count for that loop's node bucket)")
    print()
    print("same statistic at each fixed threshold, size ignored:")
    for t in THRESHOLDS:
        print("  trips >= %-4d  iterations %12d  %6.2f%%   entries %d"
              % (t, fixed_num[t], 100.0 * fixed_num[t] / tot_iters
                 if tot_iters else 0.0, fixed_ent[t]))

    rows.sort(reverse=True)
    if rows:
        top1 = rows[0]
        top10 = sum(r[0] for r in rows[:10])
        rest_it = tot_iters - top1[0]
        rest_num = weighted_num - top1[3]["gew"][top1[5]]
        print()
        print("concentration: the hottest loop (id=%d, %s) is %.1f%% of all "
              "iterations; the top 10 are %.1f%%"
              % (top1[1], top1[2]["fn"], 100.0 * top1[0] / tot_iters,
                 100.0 * top10 / tot_iters))
        print("  weighted fraction with that one loop removed  %.2f%%"
              % (100.0 * rest_num / rest_it if rest_it else 0.0))

    print()
    print("top %d loops by iterations" % top)
    for it, i, lp, tr, est, thr in rows[:top]:
        print("  id=%-5d %-28s %s:%s %-5s d=%d nodes~%-4d thr=%-4d "
              "entries=%-9d iters=%-12d mean=%.1f hot=%.1f%%"
              % (i, lp["fn"][:28], os.path.basename(lp["file"].rsplit(":", 1)[0]),
                 lp["file"].rsplit(":", 1)[1], lp["kind"], lp["depth"], est, thr,
                 tr["entries"], it, it / tr["exits"] if tr["exits"] else 0.0,
                 100.0 * tr["gew"][thr] / it if it else 0.0))

    print()
    print("top %d functions by iterations" % top)
    for fn, a in sorted(per_fn.items(), key=lambda kv: -kv[1]["iters"])[:top]:
        print("[loop-fn] fn=%-30s loops=%-4d entries=%-10d iters=%-12d "
              "lost=%-6d hot=%.1f%%"
              % (fn[:30], a["loops"], a["entries"], a["iters"], a["lost"],
                 100.0 * a["hot"] / a["iters"] if a["iters"] else 0.0))

    if want_json:
        json.dump({
            "loops": len(loops), "entered": len(rows), "entries": tot_entries,
            "iters": tot_iters, "lost": tot_lost, "zero": tot_zero,
            "hist": hist, "hist_names": names, "iters_by_nodes": nodes_hist,
            "bytes_per_node": bpn, "weighted_fraction": wf,
            "fixed": {str(t): fixed_num[t] for t in THRESHOLDS},
        }, open(want_json, "w"), indent=1)
        print("\nwrote %s" % want_json)
    return 0


def selftest(bdir):
    """Positive control: a program whose trip counts are known by construction."""
    mcc = find_mcc(bdir)
    if not os.access(mcc, os.X_OK):
        print("loop-census: no mcc at %s" % mcc)
        return 77
    prog = os.path.join(ROOT, "tests", "loopcensus", "known_trips.c")
    exp = os.path.join(ROOT, "tests", "loopcensus", "known_trips.expect")
    if not os.path.exists(prog) or not os.path.exists(exp):
        print("loop-census: missing %s" % prog)
        return 1
    want = {}
    for ln in open(exp):
        m = re.match(r"id=(\d+)\s+entries=(\d+)\s+iters=(\d+)"
                     r"\s+lost=(\d+)\s+bucket=(-?\d+)", ln.strip())
        if m:
            want[int(m.group(1))] = tuple(int(m.group(k)) for k in (2, 3, 4, 5))
    if not want:
        print("loop-census: no expectations in %s" % exp)
        return 1
    bad = 0
    with tempfile.TemporaryDirectory() as work:
        for opt in ("O0", "O1", "O2", "O3"):
            binp = os.path.join(work, "kt" + opt)
            lcf = os.path.join(work, "lc" + opt + ".txt")
            mapf = os.path.join(work, "map" + opt + ".txt")
            env = dict(os.environ)
            env["MCC_LOOP_CENSUS_MAP"] = mapf
            env.pop("MCC_LOOP_CENSUS", None)
            p = subprocess.run([mcc, "-" + opt, "-floop-census", prog,
                                "-o", binp], cwd=ROOT, env=env,
                               capture_output=True, text=True)
            if p.returncode != 0:
                print(p.stdout + p.stderr)
                print("loop-census selftest: compile FAILED at -" + opt)
                return 1
            env2 = dict(os.environ)
            env2["MCC_LOOP_CENSUS"] = lcf
            env2.pop("MCC_LOOP_CENSUS_MAP", None)
            p = subprocess.run([binp], cwd=ROOT, env=env2, capture_output=True,
                               text=True)
            if p.returncode != 0:
                print("loop-census selftest: program exited %d at -%s"
                      % (p.returncode, opt))
                return 1
            trips, tot = parse_trips(open(lcf, errors="replace").read())
            loops = parse_map(open(mapf, errors="replace").read())
            for i, (e, it, lost, bkt) in sorted(want.items()):
                tr = trips.get(i)
                if tr is None:
                    print("  -%s id=%d MISSING" % (opt, i))
                    bad += 1
                    continue
                got = (tr["entries"], tr["iters"], tr["lost"])
                hb = [j for j, v in enumerate(tr["h"]) if v]
                ok = got == (e, it, lost) and (hb == [bkt] or
                                               (lost and not tr["exits"]))
                print("  -%s id=%-2d %-6s entries=%-3d iters=%-4d lost=%-2d "
                      "h=%s  %s"
                      % (opt, i, loops.get(i, {}).get("kind", "?"), got[0],
                         got[1], got[2], hb, "ok" if ok else
                         "MISMATCH want entries=%d iters=%d lost=%d bucket=%d"
                         % (e, it, lost, bkt)))
                if not ok:
                    bad += 1
            if len(trips) != len(want):
                print("  -%s: %d [trip] rows, expected %d"
                      % (opt, len(trips), len(want)))
                bad += 1

        print("\n-- negative control: the same program without -floop-census")
        binp = os.path.join(work, "kt-off")
        lcf = os.path.join(work, "lc-off.txt")
        p = subprocess.run([mcc, "-O2", prog, "-o", binp], cwd=ROOT,
                           capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout + p.stderr)
            return 1
        env2 = dict(os.environ)
        env2["MCC_LOOP_CENSUS"] = lcf
        subprocess.run([binp], cwd=ROOT, env=env2, capture_output=True)
        if os.path.exists(lcf):
            print("  FAIL: uninstrumented binary still dumped a census")
            bad += 1
        else:
            print("  ok: no census file, so the flag is what produces the data")

        print("-- inertness control: perturbing the source moves the numbers")
        pert = os.path.join(work, "pert.c")
        srctxt = open(prog).read()
        pert_txt = srctxt.replace("#define N_OUTER 4", "#define N_OUTER 9")
        if pert_txt == srctxt:
            print("  FAIL: could not perturb the control program")
            return 1
        open(pert, "w").write(pert_txt)
        binp = os.path.join(work, "pert")
        lcf = os.path.join(work, "pert.txt")
        p = subprocess.run([mcc, "-O2", "-floop-census", pert, "-o", binp],
                           cwd=ROOT, capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout + p.stderr)
            return 1
        env2 = dict(os.environ)
        env2["MCC_LOOP_CENSUS"] = lcf
        subprocess.run([binp], cwd=ROOT, env=env2, capture_output=True)
        trips, _ = parse_trips(open(lcf, errors="replace").read())
        inner = trips.get(4)
        if inner is None or inner["entries"] != 9 or inner["iters"] != 63:
            print("  FAIL: perturbed run gave %s, wanted entries=9 iters=63"
                  % (inner,))
            bad += 1
        else:
            print("  ok: N_OUTER 4->9 moved id=4 to entries=9 iters=63")

    print("\nloop-census selftest: %s" % ("FAILED (%d)" % bad if bad else "ok"))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bdir")
    ap.add_argument("--source", default="src/mcc.c")
    ap.add_argument("--levels", default="O2")
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--json")
    ap.add_argument("--keep")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--opt-in", action="store_true")
    a = ap.parse_args()
    bdir = a.bdir if os.path.isabs(a.bdir) else os.path.join(ROOT, a.bdir)
    if a.selftest:
        sys.exit(selftest(bdir))
    if a.opt_in and not os.environ.get("MCC_LOOP_CENSUS_RUN"):
        print("loop-census: set MCC_LOOP_CENSUS_RUN=1 to run this census "
              "(it self-compiles twice)")
        sys.exit(77)
    rc = 0
    for opt in a.levels.split(","):
        rc |= run(bdir, a.source, opt, a.top, a.keep, a.json)
    sys.exit(rc)


if __name__ == "__main__":
    main()
