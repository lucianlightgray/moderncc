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

The compiler also answers `par=` per loop.  `ast_loop_parallel_legal` runs at
`ast_func_end`, where the arena exists, and writes a `[loopar] id= par=` record
that refines the `par=?` on the static `[loop]` line to `1` (provably no
dependence carried by this loop), `0` (a carried dependence is *proven*) or `?`
(the analysis declined).  `?` is reported separately and never folded into `0`.
`-O0` builds no arena, so every loop is `?` there.  The statistic that follows is
the *parallel-legal* iteration-weighted fraction: of all executed iterations,
the share in loop entries that both clear their own break-even and sit in a
par=1 loop.  Both are printed side by side, because the gap between them is the
whole point.

Every par= line also carries `why=`, the reason the predicate reached its
verdict.  That is what bounds the par=? bucket: a reason that names a weakness
of the analysis (`bases-may-alias-indirect`, `ref-not-affine`,
`subscript-not-comparable`) is convertible in principle by a stronger
predicate, while one that names a property of the program (`body-unsafe` -- the
loop calls a function; `not-analyzable` -- the loop contains a label or goto)
is not, and no amount of dependence arithmetic will move it.

`--alias-oracle` puts a *measured* ceiling on the convertible half.  It passes
`-fdep-alias-oracle`, which makes the dependence code assume two distinct base
symbols never alias even when the address chain went through a load.  That
assumption is UNSOUND in general -- `p[i]` and `q[i]` through two global
pointers have distinct base symbols and may be the same memory -- and it is
safe to ship only because `ast_loop_parallel_legal` has no caller outside this
census and `-fdump-loopdep`, so it cannot reach emitted code.  Its purpose is
to answer "if alias disambiguation were perfect, how much of par=? would
actually become par=1", which is a question the reason histogram alone cannot
answer, because a loop unblocked at the alias step can still be refused at the
next one.  Never read an --alias-oracle number as a property of the workload;
it is a property of the ceiling.

Usage:
  tools/loop-census.py <build-dir> --partest
      the control for par=: tests/loopcensus/known_deps.c has loops whose
      dependence structure is known by construction, known_deps.expect holds the
      verdict each must get, and the cell asserts separately that no carried
      loop ever comes back par=1.  Plus a negative control (-O0 -> all par=?)
      and a perturbation (drop the carry -> the verdict flips).

  tools/loop-census.py <build-dir> --selftest
      the positive control: tests/loopcensus/known_trips.c has trip counts that
      are known by construction, and known_trips.expect holds them.  Checked at
      -O0/-O1/-O2/-O3, with a negative control (no flag -> no data) and a
      perturbation (move a loop bound -> the histogram moves).

  tools/loop-census.py <build-dir> [--source src/mcc.c] [--top N]
                       [--levels O2] [--json FILE] [--keep DIR] [--opt-in]
      the self-compile census.  --opt-in makes it exit 77 unless
      MCC_LOOP_CENSUS_RUN=1, because it self-compiles three times.

  tools/loop-census.py <build-dir> --corpus runtime [--top N] [--opt-in]
      the same census over a NUMERIC workload instead of the compiler.  A
      compiler self-compile is pointer-chasing, allocation and switch dispatch,
      so it is close to the worst case for data parallelism and cannot on its
      own decide whether a lane source exists anywhere.  The corpus is
      tools/runtime-bench.py's KERNELS table, imported rather than restated so
      it cannot be curated here, and its per-kernel argv is used unchanged.
      Every statistic the self-compile prints is printed here, plus a
      per-program table, because a corpus number carried by one program is
      exactly what a second workload is supposed to expose.
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

ORACLE = []


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
            "par": m.get("par", "?"),
        }
    for ln in txt.splitlines():
        f = ln.split()
        if not f or f[0] != "[loopar]":
            continue
        m = dict(kv.split("=", 1) for kv in f[1:] if "=" in kv)
        try:
            i = int(m["id"])
        except (KeyError, ValueError):
            continue
        if i in loops and m.get("par") in ("0", "1", "?"):
            loops[i]["par"] = m["par"]
            loops[i]["why"] = m.get("why", "?")
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
        p = subprocess.run([mcc, *flags, "-" + opt, "-floop-census", *ORACLE,
                            "-c", os.path.join(ROOT, src), "-o", obj],
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


PROG_ID_STRIDE = 1000000


def runtime_corpus():
    """The numeric corpus, taken verbatim from tools/runtime-bench.py.

    Not hand-picked here on purpose.  `KERNELS` in that file is the roster the
    runtime benchmark already uses, fixed long before this question was asked
    and chosen to exercise codegen -- integer divide, switch dispatch, struct
    copy, call depth, narrowing, string work -- not to look parallel.  Importing
    it rather than restating it means the corpus cannot be curated to produce
    lanes, which is the failure mode a second workload exists to rule out.  The
    per-kernel argv is theirs too, so the sizes are not chosen here either.
    Kernels whose source is absent (the vendor/plb ones) are skipped and named.
    """
    ldr = machinery.SourceFileLoader(
        "runtime_bench", os.path.join(ROOT, "tools", "runtime-bench.py"))
    spec = importlib_util.spec_from_loader(ldr.name, ldr)
    mod = importlib_util.module_from_spec(spec)
    ldr.exec_module(mod)
    progs, missing = [], []
    for name, path, args, cflags in mod.KERNELS:
        if os.path.exists(path):
            progs.append((os.path.relpath(path, ROOT), args, cflags))
        else:
            missing.append(name)
    if missing:
        print("loop-census: kernels absent from this checkout, skipped: %s"
              % " ".join(missing))
    return progs


def run_one_program(mcc, src, args, cflags, opt, work, tag):
    """Compile one standalone program with -floop-census, run it, read it back.

    Same three steps as the self-compile, minus the self-compile: the program
    under census IS the workload, so the instrumented binary is the program
    rather than an instrumented copy of mcc.  Returns (loops, trips, tot, bpn)
    with each loop's node estimate already resolved against that program's own
    bytes-per-node calibration, so programs with different code density stay
    comparable when they are merged.
    """
    mapf = os.path.join(work, tag + ".map")
    lcf = os.path.join(work, tag + ".lc")
    binp = os.path.join(work, tag + (".exe" if mcc.endswith(".exe") else ""))
    for p in (mapf, lcf):
        if os.path.exists(p):
            os.remove(p)
    env = dict(os.environ)
    for k in ("MCC_LOOP_CENSUS", "MCC_SLICE_CENSUS", "MCC_RIR_PROD",
              "MCC_RIR_PROD_OUT", "MCC_REPLAY_IR", "MCC_TEST_OPT"):
        env.pop(k, None)
    env["MCC_LOOP_CENSUS_MAP"] = mapf
    p = subprocess.run([mcc, "-" + opt, "-floop-census", *ORACLE, *cflags, src,
                        "-o", binp, "-lm"], cwd=ROOT, env=env,
                       capture_output=True, text=True)
    if p.returncode != 0:
        print(p.stdout[-3000:] + p.stderr[-3000:])
        print("loop-census: %s FAILED to build" % src)
        return None
    loops = parse_map(open(mapf, errors="replace").read()
                      if os.path.exists(mapf) else "")
    if not loops:
        print("loop-census: %s emitted no [loop] records" % src)
        return None
    env2 = dict(os.environ)
    env2.pop("MCC_LOOP_CENSUS_MAP", None)
    env2["MCC_LOOP_CENSUS"] = lcf
    p = subprocess.run([binp] + list(args), cwd=work, env=env2,
                       capture_output=True, text=True)
    if p.returncode != 0:
        print(p.stdout[-2000:] + p.stderr[-2000:])
        print("loop-census: %s exited %d" % (src, p.returncode))
        return None
    if not os.path.exists(lcf):
        print("loop-census: %s produced no census dump" % src)
        return None
    trips, tot = parse_trips(open(lcf, errors="replace").read())
    bpn, _ = calibrate_bytes_per_node(mcc, [], src, opt, work)
    for lp in loops.values():
        lp["est"] = (max(1, int(round(lp["bytes"] / bpn))) if bpn
                     else lp["toks"])
    return loops, trips, tot, bpn


def run_corpus(bdir, progs, opt, top, keep, want_json):
    """The same census over a corpus of standalone programs.

    Every statistic the self-compile row reports is reported here over the
    merged corpus, so the two are directly comparable; ids are offset per
    program so they cannot collide.  A per-program table comes first, because a
    corpus number that one program carries is the failure mode this whole
    measurement exists to catch.
    """
    mcc = find_mcc(bdir)
    if not os.access(mcc, os.X_OK):
        print("loop-census: no mcc at %s" % mcc)
        return 77
    all_loops, all_trips, per_prog = {}, {}, []
    ctx = tempfile.TemporaryDirectory() if not keep else _KeepDir(keep)
    with ctx as work:
        for k, (rel, args, cflags) in enumerate(progs):
            src = rel if os.path.isabs(rel) else os.path.join(ROOT, rel)
            if not os.path.exists(src):
                print("loop-census: missing %s" % src)
                return 1
            print("loop-census: [%d/%d] %s %s"
                  % (k + 1, len(progs), rel, " ".join(args)))
            got = run_one_program(mcc, src, args, cflags, opt, work,
                                  "p%02d" % k)
            if got is None:
                return 1
            loops, trips, _tot, bpn = got
            base = (k + 1) * PROG_ID_STRIDE
            kern = os.path.basename(rel).rsplit(".", 1)[0]
            for i, lp in loops.items():
                lp["prog"] = rel
                lp["fn"] = "%s/%s" % (kern, lp["fn"])
                all_loops[base + i] = lp
            for i, tr in trips.items():
                all_trips[base + i] = tr
            per_prog.append((rel, tally(loops, trips, bpn), bpn))

    print()
    print("=== per-program (each compiled at -%s, run once) ===" % opt)
    print("%-46s %11s %13s %7s %7s %7s"
          % ("program", "entered", "iterations", "raw%", "par1%", "par?%"))
    for rel, t, _bpn in per_prog:
        it = t["iters"]
        print("%-46s %11d %13d %6.2f%% %6.2f%% %6.2f%%"
              % (rel[-46:], len(t["rows"]), it,
                 100.0 * t["weighted_num"] / it if it else 0.0,
                 100.0 * t["par_num"]["1"] / it if it else 0.0,
                 100.0 * t["par_iters"]["?"] / it if it else 0.0))

    merged = tally(all_loops, all_trips, None)
    tot_it = merged["iters"]
    if tot_it:
        hot = max(per_prog, key=lambda x: x[1]["iters"])
        rest_it = tot_it - hot[1]["iters"]
        rest_par = merged["par_num"]["1"] - hot[1]["par_num"]["1"]
        rest_raw = merged["weighted_num"] - hot[1]["weighted_num"]
        print()
        print("largest contributing PROGRAM: %s, %.1f%% of all iterations"
              % (hot[0], 100.0 * hot[1]["iters"] / tot_it))
        print("  corpus raw fraction without it            %.2f%%"
              % (100.0 * rest_raw / rest_it if rest_it else 0.0))
        print("  corpus parallel-legal fraction without it %.2f%% of the "
              "remaining, %.2f%% of all"
              % (100.0 * rest_par / rest_it if rest_it else 0.0,
                 100.0 * rest_par / tot_it))

    tot = {"entries": merged["entries"], "exits": 0, "lost": merged["lost"],
           "stray": merged["stray"], "iters": merged["iters"], "overflow": 0}
    rc = report(all_loops, all_trips, tot, None, -1, top, opt,
                "%d-program corpus" % len(progs), want_json)
    return rc


class _KeepDir:
    def __init__(self, d):
        self.d = d
        os.makedirs(d, exist_ok=True)

    def __enter__(self):
        return self.d

    def __exit__(self, *a):
        return False


def loop_nodes(lp, bpn):
    if "est" in lp:
        return lp["est"]
    return max(1, int(round(lp["bytes"] / bpn))) if bpn else lp["toks"]


def tally(loops, trips, bpn):
    names = bucket_names()
    t = {
        "hist": [0] * len(names), "nodes_hist": [0] * len(names),
        "iters": 0, "entries": 0, "lost": 0, "zero": 0, "stray": 0,
        "stray_loops": [], "per_fn": {}, "weighted_num": 0, "unmapped": 0,
        "rows": [], "names": names,
        "fixed_num": {x: 0 for x in THRESHOLDS},
        "fixed_ent": {x: 0 for x in THRESHOLDS},
        "par_static": {"1": 0, "0": 0, "?": 0},
        "par_entered": {"1": 0, "0": 0, "?": 0},
        "par_entries": {"1": 0, "0": 0, "?": 0},
        "par_iters": {"1": 0, "0": 0, "?": 0},
        "par_num": {"1": 0, "0": 0, "?": 0},
        "why": {},
    }
    for lp in loops.values():
        t["par_static"][lp.get("par", "?")] += 1
    for i, tr in trips.items():
        lp = loops.get(i)
        if lp is None:
            t["unmapped"] += 1
            continue
        for j, v in enumerate(tr["h"]):
            t["hist"][j] += v
        t["iters"] += tr["iters"]
        t["entries"] += tr["entries"]
        t["lost"] += tr["lost"]
        t["stray"] += tr["stray"]
        t["zero"] += tr["zero"]
        if tr["stray"]:
            t["stray_loops"].append((tr["stray"], i, lp, tr))
        est = loop_nodes(lp, bpn)
        t["nodes_hist"][bucket(est)] += tr["iters"]
        thr = breakeven_trips(est)
        t["weighted_num"] += tr["gew"][thr]
        for x in THRESHOLDS:
            t["fixed_num"][x] += tr["gew"][x]
            t["fixed_ent"][x] += tr["ge"][x]
        a = t["per_fn"].setdefault(lp["fn"], {"loops": 0, "entries": 0,
                                              "iters": 0, "lost": 0, "hot": 0})
        a["loops"] += 1
        a["entries"] += tr["entries"]
        a["iters"] += tr["iters"]
        a["lost"] += tr["lost"]
        a["hot"] += tr["gew"][thr]
        pv = lp.get("par", "?")
        t["par_entered"][pv] += 1
        t["par_entries"][pv] += tr["entries"]
        t["par_iters"][pv] += tr["iters"]
        t["par_num"][pv] += tr["gew"][thr]
        w = t["why"].setdefault((pv, lp.get("why", "?")),
                                {"loops": 0, "iters": 0, "hot": 0})
        w["loops"] += 1
        w["iters"] += tr["iters"]
        w["hot"] += tr["gew"][thr]
        t["rows"].append((tr["iters"], i, lp, tr, est, thr))
    t["rows"].sort(reverse=True)
    return t


def report(loops, trips, tot, bpn, ncal, top, opt, src, want_json):
    ta = tally(loops, trips, bpn)
    names = ta["names"]
    hist, nodes_hist = ta["hist"], ta["nodes_hist"]
    tot_iters, tot_entries = ta["iters"], ta["entries"]
    tot_lost, tot_zero, tot_stray = ta["lost"], ta["zero"], ta["stray"]
    stray_loops, per_fn, rows = ta["stray_loops"], ta["per_fn"], ta["rows"]
    weighted_num, unmapped = ta["weighted_num"], ta["unmapped"]
    fixed_num, fixed_ent = ta["fixed_num"], ta["fixed_ent"]
    par_static, par_entered = ta["par_static"], ta["par_entered"]
    par_entries, par_iters, par_num = (ta["par_entries"], ta["par_iters"],
                                       ta["par_num"])

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
    elif ncal < 0:
        print("bytes-per-node calibration  per-program (each TU calibrated on "
              "its own MCC_SLICE_CENSUS ratio)")
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

    pwf = 100.0 * par_num["1"] / tot_iters if tot_iters else 0.0
    print()
    print("=== parallel legality (ast_loop_parallel_legal) ===")
    print("par=  static loops   entered   entries        iterations      "
          "share   at break-even")
    for pv, lab in (("1", "yes"), ("0", "no"), ("?", "unknown")):
        print("  %-3s %-12d %-9d %-14d %-14d %6.2f%%  %14d"
              % (lab, par_static[pv], par_entered[pv], par_entries[pv],
                 par_iters[pv],
                 100.0 * par_iters[pv] / tot_iters if tot_iters else 0.0,
                 par_num[pv]))
    print("  (par=? is an honest refusal: the analysis could not decide, and")
    print("   is never collapsed into par=0)")

    print()
    print("why the predicate answered as it did, iteration-weighted")
    print("  this is the bound on par=?: a reason that names a WEAKNESS of the")
    print("  analysis is convertible by a stronger predicate; one that names a")
    print("  property of the PROGRAM is not")
    print("  %-4s %-26s %6s %13s %8s %13s"
          % ("par", "reason", "loops", "iterations", "share", "at break-even"))
    for (pv, why), w in sorted(ta["why"].items(),
                               key=lambda kv: -kv[1]["iters"]):
        print("  %-4s %-26s %6d %13d %7.2f%% %13d"
              % (pv, why, w["loops"], w["iters"],
                 100.0 * w["iters"] / tot_iters if tot_iters else 0.0,
                 w["hot"]))
    print()
    print("PARALLEL-LEGAL ITERATION-WEIGHTED FRACTION                   %.2f%%"
          % pwf)
    print("  (share of executed iterations in loop entries that BOTH meet the")
    print("   break-even for their node bucket AND sit in a par=1 loop)")
    print("  raw (dependence ignored)  %.2f%%   legal-only  %.2f%%"
          % (wf, pwf))
    print()
    print("raw vs legal-only at each fixed threshold:")
    for t in THRESHOLDS:
        lo = sum(tr["gew"][t] for _, _, lp, tr, _, _ in rows
                 if lp.get("par") == "1")
        print("  trips >= %-4d  raw %12d  %6.2f%%   legal %12d  %6.2f%%"
              % (t, fixed_num[t],
                 100.0 * fixed_num[t] / tot_iters if tot_iters else 0.0,
                 lo, 100.0 * lo / tot_iters if tot_iters else 0.0))

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
        rest_par = par_num["1"] - (top1[3]["gew"][top1[5]]
                                   if top1[2].get("par") == "1" else 0)
        print("  parallel-legal fraction with that one loop removed  %.2f%% "
              "of the remaining iterations, %.2f%% of all"
              % (100.0 * rest_par / rest_it if rest_it else 0.0,
                 100.0 * rest_par / tot_iters if tot_iters else 0.0))

    par_rows = [r for r in rows if r[2].get("par") == "1"]
    if par_rows:
        print()
        print("top %d par=1 loops by iterations -- the whole lane source"
              % min(top, len(par_rows)))
        for it, i, lp, tr, est, thr in par_rows[:top]:
            print("  id=%-5d %-28s %s:%s %-5s nodes~%-4d thr=%-4d "
                  "entries=%-9d iters=%-12d %5.2f%% of all"
                  % (i, lp["fn"][:28],
                     os.path.basename(lp["file"].rsplit(":", 1)[0]),
                     lp["file"].rsplit(":", 1)[1], lp["kind"], est, thr,
                     tr["entries"], it,
                     100.0 * it / tot_iters if tot_iters else 0.0))

    print()
    print("top %d loops by iterations" % top)
    for it, i, lp, tr, est, thr in rows[:top]:
        print("  id=%-5d %-28s %s:%s %-5s d=%d nodes~%-4d thr=%-4d "
              "entries=%-9d iters=%-12d mean=%.1f hot=%.1f%% par=%s"
              % (i, lp["fn"][:28], os.path.basename(lp["file"].rsplit(":", 1)[0]),
                 lp["file"].rsplit(":", 1)[1], lp["kind"], lp["depth"], est, thr,
                 tr["entries"], it, it / tr["exits"] if tr["exits"] else 0.0,
                 100.0 * tr["gew"][thr] / it if it else 0.0,
                 lp.get("par", "?")))

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
            "par_static": par_static, "par_entered": par_entered,
            "par_entries": par_entries, "par_iters": par_iters,
            "parallel_legal_weighted_fraction": pwf,
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


def partest(bdir):
    """Positive/negative control for the par= verdict.

    tests/loopcensus/known_deps.c holds loops whose dependence structure is
    known by construction; known_deps.expect holds the verdict each one must
    get.  A loop that carries a dependence must never come back par=1 -- that
    is the failure this control exists to catch.  par=? is accepted only where
    the expectation says so.
    """
    mcc = find_mcc(bdir)
    if not os.access(mcc, os.X_OK):
        print("loop-census: no mcc at %s" % mcc)
        return 77
    prog = os.path.join(ROOT, "tests", "loopcensus", "known_deps.c")
    exp = os.path.join(ROOT, "tests", "loopcensus", "known_deps.expect")
    if not os.path.exists(prog) or not os.path.exists(exp):
        print("loop-census: missing %s" % prog)
        return 1
    want = {}
    for ln in open(exp):
        m = re.match(r"id=(\d+)\s+par=([01?])\s+fn=(\S+)", ln.strip())
        if m:
            want[int(m.group(1))] = (m.group(2), m.group(3))
    if not want:
        print("loop-census: no expectations in %s" % exp)
        return 1
    bad = 0
    with tempfile.TemporaryDirectory() as work:
        for opt in ("O1", "O2", "O3"):
            got = _par_of(mcc, prog, opt, work)
            if got is None:
                print("loop-census partest: compile FAILED at -" + opt)
                return 1
            if len(got) != len(want):
                print("  -%s: %d [loop] records, expected %d"
                      % (opt, len(got), len(want)))
                bad += 1
            for i, (pv, fn) in sorted(want.items()):
                g = got.get(i)
                if g is None:
                    print("  -%s id=%d MISSING" % (opt, i))
                    bad += 1
                    continue
                ok = g["par"] == pv and g["fn"] == fn
                print("  -%s id=%-3d %-24s par=%s  %s"
                      % (opt, i, g["fn"][:24], g["par"],
                         "ok" if ok else "MISMATCH want par=%s fn=%s"
                         % (pv, fn)))
                if not ok:
                    bad += 1
            wrong = [i for i, g in got.items()
                     if g["par"] == "1" and want.get(i, ("?",))[0] != "1"]
            if wrong:
                print("  -%s UNSOUND: par=1 on carried loops %s" % (opt, wrong))
                bad += 1

        print("\n-- negative control: -O0 builds no AST, so nothing is claimed")
        got = _par_of(mcc, prog, "O0", work)
        if got is None:
            return 1
        claimed = [i for i, g in got.items() if g["par"] != "?"]
        if claimed:
            print("  FAIL: -O0 answered par= for %s" % claimed)
            bad += 1
        else:
            print("  ok: every loop is par=? at -O0")

        print("-- inertness control: removing the carry makes the verdict move")
        pert = os.path.join(work, "pert.c")
        txt = open(prog).read()
        ptxt = txt.replace("a[i] = a[i - 1] + 1;", "a[i] = b[i - 1] + 1;")
        ptxt = ptxt.replace("s += a[i];", "s = a[i];")
        if ptxt == txt:
            print("  FAIL: could not perturb the control program")
            return 1
        open(pert, "w").write(ptxt)
        got = _par_of(mcc, pert, "O2", work)
        if got is None:
            return 1
        moved = got.get(1, {}).get("par"), got.get(3, {}).get("par")
        if moved != ("1", "1"):
            print("  FAIL: perturbed run gave id=1 par=%s id=3 par=%s, "
                  "wanted 1 and 1" % moved)
            bad += 1
        else:
            print("  ok: dropping the a[i-1] read and the += flipped both to "
                  "par=1, so the predicate reads the dependence and not the "
                  "loop shape")

    print("\nloop-census partest: %s" % ("FAILED (%d)" % bad if bad else "ok"))
    return 1 if bad else 0


def _par_of(mcc, prog, opt, work):
    mapf = os.path.join(work, "pmap" + opt + ".txt")
    if os.path.exists(mapf):
        os.remove(mapf)
    env = dict(os.environ)
    env["MCC_LOOP_CENSUS_MAP"] = mapf
    env.pop("MCC_LOOP_CENSUS", None)
    p = subprocess.run([mcc, "-" + opt, "-floop-census", "-c", prog,
                        "-o", os.path.join(work, "kd" + opt + ".o")],
                       cwd=ROOT, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        print(p.stdout + p.stderr)
        return None
    if not os.path.exists(mapf):
        print("loop-census: no map at %s" % mapf)
        return None
    return parse_map(open(mapf, errors="replace").read())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bdir")
    ap.add_argument("--source", default="src/mcc.c")
    ap.add_argument("--levels", default="O2")
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--json")
    ap.add_argument("--keep")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--partest", action="store_true")
    ap.add_argument("--opt-in", action="store_true")
    ap.add_argument("--corpus", choices=["self", "runtime"], default="self")
    ap.add_argument("--program", action="append", default=[])
    ap.add_argument("--alias-oracle", action="store_true")
    a = ap.parse_args()
    if a.alias_oracle:
        ORACLE.append("-fdep-alias-oracle")
    bdir = a.bdir if os.path.isabs(a.bdir) else os.path.join(ROOT, a.bdir)
    if a.partest:
        sys.exit(partest(bdir))
    if a.selftest:
        sys.exit(selftest(bdir))
    if a.opt_in and not os.environ.get("MCC_LOOP_CENSUS_RUN"):
        print("loop-census: set MCC_LOOP_CENSUS_RUN=1 to run this census")
        sys.exit(77)
    if a.program:
        progs = [(shlex.split(x)[0], shlex.split(x)[1:], []) for x in a.program]
    elif a.corpus == "runtime":
        progs = runtime_corpus()
    else:
        progs = None
    rc = 0
    for opt in a.levels.split(","):
        if progs is None:
            rc |= run(bdir, a.source, opt, a.top, a.keep, a.json)
        else:
            rc |= run_corpus(bdir, progs, opt, a.top, a.keep, a.json)
    sys.exit(rc)


if __name__ == "__main__":
    main()
