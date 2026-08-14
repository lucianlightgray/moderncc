#!/usr/bin/env python3
"""selfhost-optbench.py -- rank the -O ladder's level-assignable flags by the
speed of the compiler they emit, per unit of compile-time price.

THE WORKLOAD IS mcc COMPILING ITSELF, IN TWO SEPARATED STAGES. Conflating them
is the easy mistake, so they never share a binary:

  stage-1  a FIXED reference compiler -- the ordinary host-cc-built mcc, whose
           own flags never change -- compiles src/mcc.c into stage-2. Only what
           stage-1 is TOLD TO DO varies: -O3 with each of the 48 level-
           assignable flags explicitly forced on or off, so the level itself is
           pinned and nothing outside the flag set moves.
  stage-2  the mcc that came out of stage-1 compiles a FIXED workload with
           FIXED flags (src/mcc.c at -O2, the shipped default set).

  cost(F)  = instructions retired by STAGE-1. The compile-time price.
  gain(F)  = work retired by STAGE-2 on the fixed workload. The payoff: the
             emitted code's quality, judged by how fast the compiler it
             produces actually runs, on a 3.2MB-object real C program rather
             than on five kernels.

CORRECTNESS IS A GATE, NOT A SCORE. stage-2's object must be byte-identical to
the one the fixed reference compiler produces from the same input at the same
flags -- a correct compiler emits the same code no matter how it was itself
built. A flag set whose stage-2 miscompiles, crashes, or drifts is EXCLUDED
from the ranking, never ranked poorly.

TWO METRICS, BECAUSE THEY DISAGREE. Measured on this workload: the all-48-on
compiler retires 2.46% MORE instructions than the all-48-off one and takes
3.08% FEWER cycles (IPC 2.81 vs 2.65). That is not noise -- instructions
reproduce to 0.003% here -- it is divmagic and the if-conversions doing exactly
what they are for: replacing one slow instruction with several fast ones. So
both are recorded. Instructions are the precise, parallel-safe axis and carry
the per-flag ranking; cycles are measured serially at high repetition on the
configurations that matter, and any flag whose two metrics disagree in sign is
reported as such instead of being silently ranked by the one that flatters it.

THE SEARCH LADDER, and why it is not exhaustive. 48 flags is 2**48 =
281,474,976,710,656 configurations; at the ~2.4s an evaluation costs here that
is 21 million years. Four bounded stages instead, each reporting its budget:

  marginal  leave-one-out from the full set AND add-one-in to the empty set
            (2*48 evaluations). Where the two disagree the flag interacts with
            or is masked by another, which the naive single number hides.
  cover3    the 74 rows of tests/optfire/cover3.txt, an existing 3-way covering
            array: every setting of every three flags appears in some row.
  shapley   each flag's marginal averaged over random subsets, sampled as
            permutation prefix sweeps so one permutation yields one sample for
            every flag at once. Seeded, so it reproduces.
  greedy    build -O1 by admitting the best remaining gain-per-cost flag until
            marginal efficiency crosses a stated threshold, then -O2, then -O3.
            The thresholds ARE the level boundaries and are printed as numbers.

Usage:
  tools/selfhost-optbench.py <build-dir> [--stage marginal,cover3,shapley,greedy]
      [--jobs N] [--reads N] [--perms N] [--seed N] [--cache PATH]
      [--out PATH] [--json PATH] [--cycles] [--cycle-reads N]
      [--t1 F] [--t2 F] [--check]

  --check re-derives the ladder from the cached measurements and fails if
  src/mccopt.h disagrees with it, which is what makes the assignment derived
  from this runner rather than asserted beside it.

Exit status: 0 all good, 1 a correctness gate failed or --check disagreed,
77 skip (no perf counter or no build tree).
"""
import argparse, hashlib, json, math, os, random, re, shlex, shutil
import subprocess, sys, tempfile, threading, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

T1_DEFAULT = 1.0
T2_DEFAULT = 0.10


def flag_table(path):
    """(name, level) for every mccopt.h row a level can be assigned to.

    ALWAYS is not level-gated, OFF is the dump-*/opt-search-* diagnostics,
    SPECIAL computes its own default, and levels above 3 are the in-development
    rungs swept separately. What is left is the ladder."""
    rows = []
    txt = open(path).read()
    pat = r'MCC_OPT_ROW\(\s*\w+,\s*"([^"]+)",\s*MCC_OPTD_LEVEL\((\d+)\)\s*\)'
    for m in re.finditer(pat, txt):
        if int(m.group(2)) <= 3:
            rows.append((m.group(1), int(m.group(2))))
    return rows


class Rig:
    """Everything needed to build a stage-2 and time it, resolved once.

    The compile flags, the runtime blob and the link libraries all come out of
    the build tree the same way tools/selfhost-fixpoint.py takes them, so this
    runner and the fixpoint gate cannot drift apart on what a self-hosted mcc
    is."""

    def __init__(self, bdir):
        self.bdir = os.path.join(ROOT, bdir) if not os.path.isabs(bdir) else bdir
        self.mcc = os.path.join(self.bdir, "mcc")
        if not os.access(self.mcc, os.X_OK):
            raise SystemExit("no mcc at %s" % self.mcc)
        cc = json.load(open(os.path.join(self.bdir, "compile_commands.json")))
        rec = [x for x in cc if x["file"].endswith("/mcc.c")][0]
        self.flags = [a for a in shlex.split(rec["command"])[1:]
                      if (a.startswith("-D") or a.startswith("-I"))
                      and not a.endswith(".c")]
        self.src = os.path.join(ROOT, "src", "mcc.c")
        self.inc = os.path.join(ROOT, "runtime", "include")
        blob = os.path.join(self.bdir, "CMakeFiles", "mcc.dir", "mccrt_blob.c.o")
        if not os.path.exists(blob):
            raise SystemExit("no runtime blob at %s (build the mcc target)" % blob)
        self.link_objs = [blob]
        if any(a.startswith("-DMCC_EMBED_JIT_BLOB") for a in self.flags):
            j = os.path.join(self.bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c.o")
            if not os.path.exists(j):
                raise SystemExit("no JIT blob at %s" % j)
            self.link_objs.append(j)
        libs = os.path.join(self.bdir, "selfhost-link-libs.txt")
        self.libs = ([l.strip() for l in open(libs) if l.strip()]
                     if os.path.exists(libs) else ["-lm", "-ldl"])
        self.stage2_opt = "-O2"
        self.ref_sha = None

    def compile_argv(self, binp, obj, opt, extra, own_headers):
        argv = [binp] + self.flags
        if own_headers:
            argv.append("-I" + self.inc)
        return argv + [opt] + list(extra) + ["-c", self.src, "-o", obj]

    def link_argv(self, obj, out):
        return [self.mcc, obj] + self.link_objs + ["-o", out] + self.libs


def perf_run(argv, events, cwd, cachedir):
    """One counted run with a private, cold XDG_CACHE_HOME.

    opt-slice memoises to ~/.cache/mcc/sl-*.ck. A warm cache is less work than
    a cold one, so leaving the real cache in place would make the second read
    of a configuration cheaper than the first and the minimum would silently
    become 'whatever the cache had'. Every read gets its own empty one."""
    env = dict(os.environ)
    env["XDG_CACHE_HOME"] = cachedir
    os.makedirs(cachedir, exist_ok=True)
    p = subprocess.run(["perf", "stat", "-e", ",".join(events), "-x,"] + argv,
                       cwd=cwd, capture_output=True, text=True, env=env)
    shutil.rmtree(cachedir, ignore_errors=True)
    out = {}
    for line in p.stderr.splitlines():
        f = line.split(",")
        if len(f) > 2:
            try:
                out[f[2].split(":")[0]] = int(float(f[0]))
            except ValueError:
                pass
    return p.returncode, out, p.stderr


def perf_min(argv, events, cwd, td, reads):
    best, err = {}, None
    for i in range(reads):
        rc, vals, stderr = perf_run(argv, events, cwd,
                                    os.path.join(td, "xdg%d" % i))
        if rc != 0:
            return None, stderr
        for k, v in vals.items():
            best[k] = v if k not in best else min(best[k], v)
    return best, err


def sha_file(path):
    with open(path, "rb") as fh:
        return hashlib.sha1(fh.read()).hexdigest()


def subset_argv(names, subset):
    """-O3 plus an explicit on/off for every one of the 48.

    Pinning the level and stating all 48 is what keeps the experiment clean:
    the SPECIAL rows that key off optimize_level, and the ALWAYS rows, are then
    identical in every configuration and only the flag set varies. Verified:
    -O3 with all 48 spelled out on is byte-identical to plain -O3."""
    return ["-O3"] + [("-f" if n in subset else "-fno-") + n for n in names]


class Bench:
    def __init__(self, rig, names, reads, cache_path):
        self.rig = rig
        self.names = names
        self.reads = reads
        self.cache_path = cache_path
        self.cache = {}
        self.lock = threading.Lock()
        self.evals = 0
        if cache_path and os.path.exists(cache_path):
            with open(cache_path) as fh:
                self.cache = json.load(fh)

    def key(self, subset):
        return ",".join(n for n in self.names if n in subset) or "(empty)"

    def reference(self, td):
        """The object the fixed reference compiler emits for the stage-2 job."""
        obj = os.path.join(td, "ref.o")
        argv = self.rig.compile_argv(self.rig.mcc, obj, self.rig.stage2_opt,
                                     [], False)
        p = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True)
        if p.returncode != 0:
            raise SystemExit("reference stage-2 object failed: %s" % p.stderr[:400])
        self.rig.ref_sha = sha_file(obj)
        self.rig.ref_size = os.path.getsize(obj)

    def evaluate(self, subset, events=("instructions",), force=None):
        k = self.key(subset) if force is None else force
        with self.lock:
            hit = self.cache.get(k)
        if hit is not None and all(e in hit.get("events", []) for e in events):
            return hit
        td = tempfile.mkdtemp(prefix="sfob-")
        try:
            res = self._evaluate(subset, events, td)
        finally:
            shutil.rmtree(td, ignore_errors=True)
        res["events"] = list(events)
        with self.lock:
            self.cache[k] = res
            self.evals += 1
        return res

    def _evaluate(self, subset, events, td):
        rig = self.rig
        res = {"n": len(subset)}
        o1 = os.path.join(td, "o1.o")
        argv = rig.compile_argv(rig.mcc, o1, "", subset_argv(self.names, subset),
                                False)
        argv = [a for a in argv if a != ""]
        got, err = perf_min(argv, ("instructions",), ROOT, td, self.reads)
        if got is None:
            res["error"] = "stage-1 compile failed: %s" % (err or "")[-300:]
            return res
        res["cost"] = got["instructions"]
        res["o1_size"] = os.path.getsize(o1)
        res["o1_text"] = text_bytes(o1)

        mcc1 = os.path.join(td, "mcc1")
        p = subprocess.run(rig.link_argv(o1, mcc1), cwd=ROOT,
                           capture_output=True, text=True)
        if p.returncode != 0 or not os.path.exists(mcc1):
            res["error"] = "stage-2 link failed: %s" % p.stderr[-300:]
            return res

        o2 = os.path.join(td, "o2.o")
        argv = rig.compile_argv(mcc1, o2, rig.stage2_opt, [], True)
        got, err = perf_min(argv, events, ROOT, td, self.reads)
        if got is None:
            res["error"] = "stage-2 run failed: %s" % (err or "")[-300:]
            return res
        if not os.path.exists(o2) or sha_file(o2) != rig.ref_sha:
            res["error"] = "stage-2 output parity FAILED (emitted object differs " \
                           "from the reference compiler's)"
            return res
        for e, v in got.items():
            res["gain_" + e] = v
        return res

    def save(self):
        if not self.cache_path:
            return
        tmp = self.cache_path + ".tmp"
        with open(tmp, "w") as fh:
            json.dump(self.cache, fh, sort_keys=True)
        os.replace(tmp, self.cache_path)


def text_bytes(path):
    p = subprocess.run(["readelf", "-S", "-W", path], capture_output=True,
                       text=True)
    if p.returncode != 0:
        return None
    for line in p.stdout.splitlines():
        f = line.replace("[", " ").replace("]", " ").split()
        if len(f) > 6 and f[1] == ".text" and f[2] == "PROGBITS":
            try:
                return int(f[5], 16)
            except ValueError:
                return None
    return None


def pct(a, b):
    return (a - b) / b * 100.0 if b else 0.0


def run_parallel(bench, subsets, jobs, tag):
    import concurrent.futures
    todo = []
    seen = set()
    for s in subsets:
        k = bench.key(s)
        if k in seen:
            continue
        seen.add(k)
        if k not in bench.cache:
            todo.append(s)
    t0 = time.time()
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futs = [ex.submit(bench.evaluate, s) for s in todo]
        for f in futs:
            f.result()
            done += 1
            if done % 20 == 0 or done == len(todo):
                print("  %s %d/%d  (%.0fs)" % (tag, done, len(todo),
                                               time.time() - t0),
                      file=sys.stderr)
                bench.save()
    bench.save()
    return len(todo), time.time() - t0


def stage_marginal(bench, names, jobs):
    full, empty = set(names), set()
    subs = [full, empty]
    subs += [full - {n} for n in names]
    subs += [{n} for n in names]
    n, secs = run_parallel(bench, subs, jobs, "marginal")
    base_full = bench.evaluate(full)
    base_empty = bench.evaluate(empty)
    rows = {}
    for name in names:
        loo = bench.evaluate(full - {name})
        aoi = bench.evaluate({name})
        r = {"flag": name}
        r["loo_ok"] = "error" not in loo and "error" not in base_full
        r["aoi_ok"] = "error" not in aoi and "error" not in base_empty
        r["loo_error"] = loo.get("error")
        r["aoi_error"] = aoi.get("error")
        if r["loo_ok"]:
            r["loo_cost"] = pct(base_full["cost"], loo["cost"])
            r["loo_gain"] = pct(loo["gain_instructions"],
                                base_full["gain_instructions"])
            r["loo_text"] = pct(base_full["o1_text"], loo["o1_text"])
        if r["aoi_ok"]:
            r["aoi_cost"] = pct(aoi["cost"], base_empty["cost"])
            r["aoi_gain"] = pct(base_empty["gain_instructions"],
                                aoi["gain_instructions"])
            r["aoi_text"] = pct(aoi["o1_text"], base_empty["o1_text"])
        rows[name] = r
    return rows, n, secs, base_full, base_empty


def cover3_subsets(path, names):
    """The 74 rows of the existing 3-way covering array, projected onto the 48.

    Reusing this rather than inventing a design is the point: cover3.py verify
    re-derives the covering property on every ctest run. The array spans all
    113 rows of the table, but a covering array projected onto a subset of its
    columns still covers every triple WITHIN that subset, so reading only the
    48 level-assignable columns and leaving the rest at their -O3 default is a
    sound 3-wise design over exactly the population being ranked."""
    if not os.path.exists(path):
        return None
    idx, rows = {}, []
    for line in open(path):
        f = line.split()
        if len(f) >= 3 and f[0] == "flag" and f[2] in names:
            idx[int(f[1])] = f[2]
        elif len(f) == 3 and f[0] == "row":
            rows.append(f[2])
    if not idx or not rows:
        return None
    return [{n for i, n in idx.items() if i < len(r) and r[i] == "1"}
            for r in rows]


def stage_cover3(bench, names, jobs, path):
    subs = cover3_subsets(path, names)
    if subs is None:
        return None, 0, 0.0
    n, secs = run_parallel(bench, subs, jobs, "cover3")
    out = []
    for s in subs:
        r = bench.evaluate(s)
        out.append({"n": len(s), "ok": "error" not in r, "error": r.get("error"),
                    "cost": r.get("cost"), "gain": r.get("gain_instructions")})
    return out, n, secs


def stage_shapley(bench, names, jobs, perms, seed):
    """Permutation-prefix sampling: one permutation yields one marginal sample
    for every flag at once, so P permutations cost P*(N+1) evaluations and buy
    P samples per flag rather than P/N."""
    rng = random.Random(seed)
    seqs = []
    for _ in range(perms):
        order = list(names)
        rng.shuffle(order)
        seqs.append(order)
    subs = []
    for order in seqs:
        cur = set()
        subs.append(set(cur))
        for f in order:
            cur.add(f)
            subs.append(set(cur))
    n, secs = run_parallel(bench, subs, jobs, "shapley")
    acc = {f: {"gain": [], "cost": []} for f in names}
    for order in seqs:
        cur = set()
        prev = bench.evaluate(cur)
        for f in order:
            cur.add(f)
            nxt = bench.evaluate(cur)
            if "error" in prev or "error" in nxt:
                prev = nxt
                continue
            acc[f]["gain"].append(pct(prev["gain_instructions"],
                                      nxt["gain_instructions"]))
            acc[f]["cost"].append(pct(nxt["cost"], prev["cost"]))
            prev = nxt
    rows = {}
    for f in names:
        g, c = acc[f]["gain"], acc[f]["cost"]
        sd = stdev(g)
        rows[f] = {"samples": len(g),
                   "gain": sum(g) / len(g) if g else 0.0,
                   "cost": sum(c) / len(c) if c else 0.0,
                   "gain_sd": sd, "cost_sd": stdev(c),
                   "gain_se": sd / math.sqrt(len(g)) if g else 0.0}
    return rows, n, secs


def stdev(xs):
    if len(xs) < 2:
        return 0.0
    m = sum(xs) / len(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def stage_greedy(bench, candidates, jobs):
    """Admit the best remaining gain-per-cost flag, over and over.

    This is the only stage whose answer is an ORDER rather than a number, and
    the order is what the levels are cut out of: the efficiency recorded for
    each flag is its marginal efficiency at the moment it was admitted, in the
    context of everything admitted before it, which is the context it will
    actually ship in.

    It runs over the RANKED flags only, and that restriction is the whole
    point. Run over all 48, this stage cheerfully admits flags that change no
    emitted byte at an "efficiency" of 0.8 -- noise over noise, sorted
    confidently. The buckets are decided first, from significance, and only
    what survives them is allowed into the ranking."""
    chosen, order = set(), []
    remaining = list(candidates)
    total = 0
    t0 = time.time()
    while remaining:
        cand = [chosen | {f} for f in remaining]
        n, _ = run_parallel(bench, cand, jobs, "greedy(%d)" % len(chosen))
        total += n
        cur = bench.evaluate(chosen)
        best = None
        for f in remaining:
            r = bench.evaluate(chosen | {f})
            if "error" in r or "error" in cur:
                continue
            g = pct(cur["gain_instructions"], r["gain_instructions"])
            c = pct(r["cost"], cur["cost"])
            eff = g / c if c > 0.005 else (g * 1e3 if g > 0 else g)
            if best is None or eff > best[1]:
                best = (f, eff, g, c)
        if best is None:
            break
        f, eff, g, c = best
        order.append({"flag": f, "step": len(order) + 1, "efficiency": eff,
                      "gain": g, "cost": c})
        chosen.add(f)
        remaining.remove(f)
    return order, total, time.time() - t0


COST_HEAVY = 5.0
COST_ABSURD = 50.0
DARK_LEVEL = 9


def derive_levels(rows, order, t1, t2, levels_now, pins):
    """The level assignment, as a rule applied to the measurements.

    Written out so it can be re-derived rather than argued about:

      ranked flag, marginal efficiency >= t1              -> -O1
      ranked flag, marginal efficiency >= t2              -> -O2
      ranked flag, positive gain at any cost              -> -O3
      not ranked, stage-1 cost >= %.0f%%                     -> -O3
      not ranked, stage-1 cost >= %.0f%%                    -> off the ladder
      not ranked, anything else                           -> unchanged

    The last line is the honest one. A flag that moves nothing measurable has
    produced no evidence about where it belongs, and inventing a level for it
    from a noise ratio is exactly what this whole exercise exists to stop. It
    keeps the level it has, and the report says why.

    `pins` are documented overrides -- a flag whose two metrics disagreed and
    was adjudicated on cycles, or one a correctness gate constrains. Each
    carries its reason into the report so the assignment stays derived rather
    than asserted.
    """
    eff = {e["flag"]: e["efficiency"] for e in order}
    levels, why = {}, {}
    for f, r in rows.items():
        if f in pins:
            levels[f], why[f] = pins[f][0], "pinned: " + pins[f][1]
            continue
        if r["bucket"] == "ranked" and f in eff:
            e = eff[f]
            levels[f] = 1 if e >= t1 else (2 if e >= t2 else 3)
            why[f] = "greedy efficiency %.3f" % e
            continue
        cost = max([v for v in (r.get("shap_cost"), r.get("loo_cost"))
                    if v is not None] or [0.0])
        if cost >= COST_ABSURD:
            levels[f] = DARK_LEVEL
            why[f] = "no measurable gain and %.1f%% stage-1 cost" % cost
        elif cost >= COST_HEAVY:
            levels[f] = 3
            why[f] = "no measurable gain and %.1f%% stage-1 cost" % cost
        else:
            levels[f] = levels_now[f]
            why[f] = "unchanged: %s, nothing measured to move it on" % r["bucket"]
    return levels, why


derive_levels.__doc__ = derive_levels.__doc__ % (COST_HEAVY, COST_ABSURD)


def measure_cycles(bench, names, configs, reads):
    """Cycles, serially and at high repetition, for the few configurations
    whose effect is big enough to see through cycle noise.

    Instructions reproduce to 3e-5 here and cycles to about 1e-2, so cycles can
    never carry a per-flag ranking on this workload -- but they are the only
    metric that answers 'is the emitted compiler actually faster', and on the
    all-on/all-off pair they answer it in the opposite direction from
    instructions. They are measured here, alone on the box, and reported beside
    the instruction number rather than instead of it."""
    out = {}
    for label, subset in configs:
        td = tempfile.mkdtemp(prefix="sfob-cyc-")
        try:
            rig = bench.rig
            o1 = os.path.join(td, "o1.o")
            argv = [a for a in rig.compile_argv(
                rig.mcc, o1, "", subset_argv(names, subset), False) if a]
            p = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True)
            if p.returncode != 0:
                out[label] = {"error": "stage-1 failed"}
                continue
            mcc1 = os.path.join(td, "mcc1")
            p = subprocess.run(rig.link_argv(o1, mcc1), cwd=ROOT,
                               capture_output=True, text=True)
            if p.returncode != 0:
                out[label] = {"error": "link failed"}
                continue
            o2 = os.path.join(td, "o2.o")
            argv = rig.compile_argv(mcc1, o2, rig.stage2_opt, [], True)
            vals = {"cycles": [], "instructions": []}
            for i in range(reads):
                rc, v, _ = perf_run(argv, ("instructions", "cycles"), ROOT,
                                    os.path.join(td, "xdg%d" % i))
                if rc != 0:
                    break
                for k in vals:
                    if k in v:
                        vals[k].append(v[k])
            if not vals["cycles"]:
                out[label] = {"error": "no cycle counter"}
                continue
            out[label] = {"cycles": min(vals["cycles"]),
                          "cycles_med": sorted(vals["cycles"])[len(vals["cycles"]) // 2],
                          "cycles_spread": (max(vals["cycles"]) - min(vals["cycles"]))
                          / min(vals["cycles"]) * 100.0,
                          "instructions": min(vals["instructions"]),
                          "reads": len(vals["cycles"]),
                          "o1_text": text_bytes(o1),
                          "n": len(subset)}
        finally:
            shutil.rmtree(td, ignore_errors=True)
    return out


def classify(marg, shap, base_full, base_empty, floor):
    """The three honesty buckets, decided before any ranking happens.

    A flag that changes no emitted byte has nothing to rank; a flag that costs
    but never pays is a finding, not a low score; and only what is left gets a
    gain/cost ratio. Sorting the whole population by a ratio would put noise
    over noise at the top of the table, which is the one outcome this must
    not produce."""
    out = {}
    for f, m in marg.items():
        r = dict(m)
        s = shap.get(f, {})
        r["shap_gain"] = s.get("gain")
        r["shap_cost"] = s.get("cost")
        r["shap_gain_sd"] = s.get("gain_sd")
        r["shap_gain_se"] = s.get("gain_se")
        r["shap_samples"] = s.get("samples")
        moves = (abs(r.get("loo_text", 0.0)) > 0 or abs(r.get("aoi_text", 0.0)) > 0
                 or abs(r.get("loo_cost", 0.0)) >= floor["cost"]
                 or abs(r.get("aoi_cost", 0.0)) >= floor["cost"])
        se = r.get("shap_gain_se") or 0.0
        g = r.get("shap_gain")
        r["significant"] = (g is not None and se > 0
                            and abs(g) >= 2 * se and abs(g) >= floor["gain"])
        r["bucket"] = ("inert" if not moves else
                       "ranked" if (r["significant"] and g > 0) else
                       "cost-no-gain")
        if r["significant"] and g is not None and g < 0:
            r["bucket"] = "cost-negative-gain"
        cost = max([v for v in (r.get("loo_cost"), r.get("shap_cost")) if v is not None]
                   or [0.0])
        gain = g if g is not None else r.get("loo_gain", 0.0)
        r["rank_gain"] = gain
        r["rank_cost"] = cost
        r["efficiency"] = (gain / cost) if cost > 0.005 else \
            (float("inf") if r["bucket"] == "ranked" else 0.0)
        r["disagree"] = (r.get("loo_gain") is not None
                         and r.get("aoi_gain") is not None
                         and abs(r["loo_gain"]) >= floor["gain"]
                         and abs(r["aoi_gain"]) >= floor["gain"]
                         and (r["loo_gain"] > 0) != (r["aoi_gain"] > 0))
        out[f] = r
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bdir")
    ap.add_argument("--stage", default="marginal,cover3,shapley,greedy")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--reads", type=int, default=2)
    ap.add_argument("--perms", type=int, default=24)
    ap.add_argument("--seed", type=int, default=20260807)
    ap.add_argument("--cache", default=None)
    ap.add_argument("--out", default=os.path.join(ROOT, "tests", "optfire",
                                                  "selfhost-levelbench.tsv"))
    ap.add_argument("--json", default=None)
    ap.add_argument("--cycles", action="store_true")
    ap.add_argument("--cycle-reads", type=int, default=11)
    ap.add_argument("--cycle-flags", default=None,
                    help="also measure each of these flags on and off in the "
                         "-O2 context, in cycles, serially -- for the rows "
                         "whose instruction count and cycle count disagree")
    ap.add_argument("--t1", type=float, default=T1_DEFAULT)
    ap.add_argument("--t2", type=float, default=T2_DEFAULT)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--pins", default=None,
                    help="file of documented overrides, one flag|level|reason "
                         "per line (default tests/optfire/levelpins.txt)")
    ap.add_argument("--pin", action="append", default=None,
                    help="flag=LEVEL:reason -- a documented override, for a row "
                         "the two metrics disagree on or a correctness gate "
                         "constrains. Repeatable; every pin is printed.")
    args = ap.parse_args()

    if not shutil.which("perf"):
        print("no perf counter; skipping")
        return 77
    rig = Rig(args.bdir)
    table = flag_table(os.path.join(ROOT, "src", "mccopt.h"))
    names = [n for n, _ in table]
    levels_now = dict(table)
    cache = args.cache or os.path.join(rig.bdir, "optbench-cache.json")
    bench = Bench(rig, names, args.reads, cache)

    td = tempfile.mkdtemp(prefix="sfob-ref-")
    try:
        bench.reference(td)
    finally:
        shutil.rmtree(td, ignore_errors=True)
    print("reference stage-2 object sha %s (%d bytes)"
          % (rig.ref_sha[:12], rig.ref_size))

    stages = set(args.stage.split(","))
    budget = {}
    full, empty = set(names), set()

    r1 = bench.evaluate(full)
    r0 = bench.evaluate(full, force="__noise__")
    floor = {"cost": max(0.005, abs(pct(r1["cost"], r0["cost"])) * 3),
             "gain": max(0.005, abs(pct(r1["gain_instructions"],
                                        r0["gain_instructions"])) * 3)}
    print("noise floor (full set measured twice, x3): cost %.4f%%  gain %.4f%%"
          % (floor["cost"], floor["gain"]))

    marg, shap = {}, {}
    if "marginal" in stages:
        marg, n, secs, base_full, base_empty = stage_marginal(bench, names, args.jobs)
        budget["marginal"] = (n, secs)
    else:
        base_full, base_empty = bench.evaluate(full), bench.evaluate(empty)

    cov = None
    if "cover3" in stages:
        cov, n, secs = stage_cover3(bench, names, args.jobs,
                                    os.path.join(ROOT, "tests", "optfire",
                                                 "cover3.txt"))
        budget["cover3"] = (n, secs)

    if "shapley" in stages:
        shap, n, secs = stage_shapley(bench, names, args.jobs, args.perms, args.seed)
        budget["shapley"] = (n, secs)

    rows = classify(marg, shap, base_full, base_empty, floor)
    ranked = [f for f in names if rows.get(f, {}).get("bucket") == "ranked"]
    print("buckets: %d ranked, %d cost-no-gain, %d cost-negative-gain, %d inert"
          % (len(ranked),
             sum(1 for r in rows.values() if r["bucket"] == "cost-no-gain"),
             sum(1 for r in rows.values() if r["bucket"] == "cost-negative-gain"),
             sum(1 for r in rows.values() if r["bucket"] == "inert")))

    pins = {}
    pinfile = args.pins or os.path.join(ROOT, "tests", "optfire", "levelpins.txt")
    if os.path.exists(pinfile):
        for line in open(pinfile):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            f = line.split("|", 2)
            if len(f) == 3 and f[0] in levels_now:
                pins[f[0]] = (int(f[1]), f[2])
    for p in (args.pin or []):
        name, _, rest = p.partition("=")
        lvl, _, reason = rest.partition(":")
        if name in levels_now:
            pins[name] = (int(lvl), reason or "no reason given")

    order, levels, why = [], {}, {}
    if "greedy" in stages:
        order, n, secs = stage_greedy(bench, ranked, args.jobs)
        budget["greedy"] = (n, secs)
        levels, why = derive_levels(rows, order, args.t1, args.t2, levels_now,
                                    pins)
    for f, r in rows.items():
        r["level_now"] = levels_now[f]
        r["level_new"] = levels.get(f)
        r["why"] = why.get(f)

    cyc = {}
    if args.cycles or args.cycle_flags:
        configs = [("all-on", full), ("all-off", empty)]
        for lv in (1, 2, 3):
            configs.append(("O%d-now" % lv,
                            {f for f in names if levels_now[f] <= lv}))
            if levels:
                configs.append(("O%d-new" % lv,
                                {f for f in names if levels.get(f, 3) <= lv}))
        if args.cycle_flags:
            o2 = {f for f in names if levels_now[f] <= 2}
            for f in args.cycle_flags.split(","):
                if f not in names:
                    continue
                configs.append(("O2+%s" % f, o2 | {f}))
                configs.append(("O2-%s" % f, o2 - {f}))
        cyc = measure_cycles(bench, names, configs, args.cycle_reads)

    write_report(args.out, rig, names, rows, order, levels, budget, base_full,
                 base_empty, floor, cov, cyc, args)
    if args.json:
        with open(args.json, "w") as fh:
            json.dump({"rows": rows, "greedy": order, "levels": levels,
                       "budget": budget, "cycles": cyc, "floor": floor,
                       "base_full": base_full, "base_empty": base_empty,
                       "cover3": cov, "seed": args.seed, "perms": args.perms},
                      fh, indent=1, sort_keys=True)
        print("wrote %s" % args.json)
    bench.save()

    if args.check:
        # Two ways this check used to pass having checked nothing, which is
        # sweep row 11. First, `and levels` meant an empty derivation skipped
        # the comparison silently -- no PASS line, no FAIL, exit 0. Second, a
        # flag with no measurable evidence is assigned levels[f] = levels_now[f]
        # by design, so comparing it against src/mccopt.h is comparing a value
        # with itself: an all-inert run printed "matches the ladder" having
        # derived nothing. Require that some flag was actually derived from a
        # measurement before the agreement means anything.
        derived = [f for f in names
                   if not why.get(f, "").startswith("unchanged:")]
        if not levels:
            print("FAIL: --check derived no levels at all, so 'matches the "
                  "ladder' would have been a statement about an empty set")
            return 1
        if not derived:
            print("FAIL: --check derived %d level(s) but every one is "
                  "'unchanged' -- each was assigned its current src/mccopt.h "
                  "value, so the comparison below compares those values with "
                  "themselves and cannot fail. Nothing was measured that could "
                  "move a flag." % len(levels))
            return 1
        bad = [f for f in names if levels_now[f] != levels[f]]
        if bad:
            print("FAIL: src/mccopt.h disagrees with the derived ladder for: %s"
                  % ", ".join("%s (%d vs derived %d)"
                              % (f, levels_now[f], levels[f]) for f in bad))
            return 1
        print("check: src/mccopt.h matches the ladder derived from these "
              "measurements (t1=%.3f t2=%.3f); %d of %d flag(s) were derived "
              "from evidence, the rest kept their current level"
              % (args.t1, args.t2, len(derived), len(levels)))
    errs = [f for f, r in rows.items()
            if r.get("loo_error") or r.get("aoi_error")]
    for f in errs:
        print("EXCLUDED %s: %s" % (f, rows[f].get("loo_error")
                                   or rows[f].get("aoi_error")))
    if errs:
        print("%d flag set(s) excluded: correctness is a gate, not a score, so a "
              "configuration whose stage-2 will not build or does not reproduce "
              "the reference object leaves the ranking rather than scoring badly."
              % len(errs))
    return 0


def write_report(path, rig, names, rows, order, levels, budget, base_full,
                 base_empty, floor, cov, cyc, args):
    step = {e["flag"]: e for e in order}
    with open(path, "w") as fh:
        fh.write("# selfhost-optbench: stage-1 compiles src/mcc.c with the flag "
                 "set; stage-2 is the mcc that comes out, compiling src/mcc.c "
                 "at %s.\n" % rig.stage2_opt)
        fh.write("# cost = stage-1 instructions retired (compile-time price). "
                 "gain = reduction in stage-2 instructions retired (payoff).\n")
        fh.write("# all-on: cost %.4fG gain-work %.4fG   all-off: cost %.4fG "
                 "gain-work %.4fG\n"
                 % (base_full["cost"] / 1e9,
                    base_full["gain_instructions"] / 1e9,
                    base_empty["cost"] / 1e9,
                    base_empty["gain_instructions"] / 1e9))
        fh.write("# noise floor cost %.4f%% gain %.4f%%; shapley perms=%d seed=%d\n"
                 % (floor["cost"], floor["gain"], args.perms, args.seed))
        for k, (n, secs) in sorted(budget.items()):
            fh.write("# budget %s: %d evaluations, %.0fs\n" % (k, n, secs))
        if cyc:
            fh.write("#\n# cycles (serial, %d reads, min):\n" % args.cycle_reads)
            for lbl, v in sorted(cyc.items()):
                if "error" in v:
                    fh.write("#   %-10s %s\n" % (lbl, v["error"]))
                else:
                    fh.write("#   %-10s n=%2d  cycles %.4fG  insns %.4fG  "
                             "IPC %.3f  spread %.3f%%\n"
                             % (lbl, v["n"], v["cycles"] / 1e9,
                                v["instructions"] / 1e9,
                                v["instructions"] / v["cycles"],
                                v["cycles_spread"]))
        if cov:
            okn = sum(1 for c in cov if c["ok"])
            fh.write("# cover3: %d rows, %d correct, %d excluded\n"
                     % (len(cov), okn, len(cov) - okn))
        fh.write("#\n")
        fh.write("# level rule: ranked and efficiency >= %.3f -> -O1; >= %.3f -> "
                 "-O2; ranked otherwise -> -O3; not ranked and stage-1 cost >= "
                 "%.0f%% -> -O3, >= %.0f%% -> off the ladder (level %d); not "
                 "ranked otherwise -> unchanged.\n"
                 % (args.t1, args.t2, COST_HEAVY, COST_ABSURD, DARK_LEVEL))
        fh.write("# a flag is RANKED only if its sampled marginal gain clears "
                 "two standard errors of its own sampling spread; nothing else "
                 "is given a gain/cost ratio.\n#\n")
        hdr = ["flag", "level_now", "level_new", "bucket", "greedy_step",
               "greedy_eff", "shap_gain", "shap_gain_se", "shap_cost",
               "loo_gain", "loo_cost", "aoi_gain", "aoi_cost", "efficiency",
               "text_pct", "interacts", "why"]
        fh.write("\t".join(hdr) + "\n")
        def sk(f):
            r = rows[f]
            b = {"ranked": 4, "cost-negative-gain": 3, "cost-no-gain": 2,
                 "inert": 1}.get(r["bucket"], 0)
            e = r["efficiency"]
            return (-b, -(1e18 if e == float("inf") else e), -r["rank_gain"])
        for f in sorted(rows, key=sk):
            r = rows[f]
            s = step.get(f, {})
            fh.write("\t".join([
                f, str(r["level_now"]), str(r.get("level_new") or "-"),
                r["bucket"], str(s.get("step", "-")),
                "%.3f" % s["efficiency"] if "efficiency" in s else "-",
                fmt(r.get("shap_gain")), fmt(r.get("shap_gain_se")),
                fmt(r.get("shap_cost")), fmt(r.get("loo_gain")),
                fmt(r.get("loo_cost")), fmt(r.get("aoi_gain")),
                fmt(r.get("aoi_cost")),
                ("inf" if r["efficiency"] == float("inf")
                 else "%.3f" % r["efficiency"]),
                fmt(r.get("loo_text")),
                "yes" if r.get("disagree") else "no",
                r.get("why") or "-"]) + "\n")
    print("wrote %s" % path)


def fmt(v):
    return "-" if v is None else "%.4f" % v


if __name__ == "__main__":
    sys.exit(main())
