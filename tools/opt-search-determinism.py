#!/usr/bin/env python3
"""Reproducibility gate for the -O search tier (-O13, the one search rung).

The search tier used to be driven by a wall-clock budget: MCC_OPT_SEARCH_LEVEL
made `-O<n>` mean "search for n seconds", so the optimizer stopped wherever the
clock happened to land and the same source compiled on a loaded machine
produced different bytes than on an idle one. That is now a tick count -- a
fixed number of candidate evaluations -- and this cell exists to keep it that
way.

It compiles one source twice at the search tier, once with the box otherwise
idle and once with the box deliberately oversubscribed, each run given its own
empty XDG_CACHE_HOME so no disk memo carries state between them, and asserts
the two objects are byte-identical. A wall-clock budget cannot pass this; a
tick count cannot fail it.

Usage:
  tools/opt-search-determinism.py <mcc> <source.c> [--opt -O13] [--load N]
                                  [--mutate] [--from-build DIR]
                                  [-- <extra mcc args...>]

Exit status: 0 byte-identical, 1 the objects differed (or --mutate and they did
not), 77 skip (no runnable mcc, no source, no os.fork).
"""
import sys, os, json, re, shlex, subprocess, tempfile, filecmp, argparse, time


def spin(stop_at):
    while time.time() < stop_at:
        pass


def start_load(n, seconds):
    pids = []
    stop_at = time.time() + seconds
    for _ in range(n):
        pid = os.fork()
        if pid == 0:
            try:
                spin(stop_at)
            finally:
                os._exit(0)
        pids.append(pid)
    return pids


def stop_load(pids):
    for p in pids:
        try:
            os.kill(p, 9)
        except OSError:
            pass
    for p in pids:
        try:
            os.waitpid(p, 0)
        except OSError:
            pass


def pin_to_one_cpu():
    try:
        cpus = sorted(os.sched_getaffinity(0))
    except AttributeError:
        return None
    if len(cpus) < 2:
        return None
    os.sched_setaffinity(0, {cpus[0]})
    return cpus


def compile_once(mcc, flags, source, out, cache, pin=False):
    env = dict(os.environ)
    env["XDG_CACHE_HOME"] = cache
    os.makedirs(cache, exist_ok=True)
    cmd = [mcc, *flags, "-c", source, "-o", out]
    pre = pin_to_one_cpu if pin else None
    t0 = time.time()
    r = subprocess.run(cmd, env=env, capture_output=True, text=True,
                       preexec_fn=pre)
    return r, time.time() - t0, cmd


def main():
    argv = sys.argv[1:]
    extra = []
    if "--" in argv:
        i = argv.index("--")
        argv, extra = argv[:i], argv[i + 1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("mcc")
    ap.add_argument("source")
    ap.add_argument("--opt", default="-O13")
    ap.add_argument("--load", type=int, default=0,
                    help="spinner processes to run against the second compile; "
                         "0 means 4x the CPU count")
    ap.add_argument("--from-build", default=None)
    ap.add_argument("--mutate", action="store_true",
                    help="the known-positive arm: give the second compile a "
                         "different tick count, so the two objects genuinely "
                         "differ and this tool must say so. Without it a run "
                         "that compared a file against itself, or never opened "
                         "the objects at all, would be indistinguishable from "
                         "a reproducible search")
    a = ap.parse_args(argv)

    if not hasattr(os, "fork"):
        print("opt-search-determinism: no os.fork on this host, so the loaded "
              "arm cannot be produced and the cell would compare two idle "
              "compiles; skipping")
        return 77
    if not os.access(a.mcc, os.X_OK):
        print(f"opt-search-determinism: no runnable mcc at {a.mcc}; skipping")
        return 77
    if not os.path.exists(a.source):
        print(f"opt-search-determinism: no source at {a.source}; skipping")
        return 77

    flags = list(extra)
    if a.from_build:
        cdb = os.path.join(a.from_build, "compile_commands.json")
        if not os.path.exists(cdb):
            print(f"opt-search-determinism: no compile_commands.json in "
                  f"{a.from_build}; skipping")
            return 77
        recs = [x for x in json.load(open(cdb)) if x["file"].endswith("/mcc.c")]
        if not recs:
            print(f"opt-search-determinism: no src/mcc.c record in {cdb}; "
                  f"skipping")
            return 77
        cmd = recs[0]["command"]
        if os.name == "nt":
            cmd = re.sub(r'\\(?!")', "/", cmd)
        flags = [x for x in shlex.split(cmd)[1:]
                 if (x.startswith("-D") or x.startswith("-I"))
                 and not x.endswith(".c")] + flags
    flags = [a.opt] + flags

    nload = a.load if a.load > 0 else max(4, (os.cpu_count() or 4) * 4)

    with tempfile.TemporaryDirectory() as work:
        idle_o = os.path.join(work, "idle.o")
        load_o = os.path.join(work, "load.o")
        warm_o = os.path.join(work, "warm.o")
        shared = os.path.join(work, "c-shared")
        r, t_idle, cmd = compile_once(a.mcc, flags, a.source, idle_o, shared)
        if r.returncode != 0:
            print(f"opt-search-determinism: the idle compile failed (exit "
                  f"{r.returncode}), so this cell cannot say anything about "
                  f"whether {a.opt} is reproducible\ncmd: {' '.join(cmd)}\n"
                  f"{r.stderr[-2000:]}")
            return 1

        load_flags = list(flags)
        if a.mutate:
            load_flags.append("-fopt-search-ticks=0")

        pids = start_load(nload, 900)
        try:
            time.sleep(1.0)
            r, t_load, cmd = compile_once(a.mcc, load_flags, a.source, load_o,
                                          os.path.join(work, "c-load"),
                                          pin=True)
        finally:
            stop_load(pids)
        if r.returncode != 0:
            print(f"opt-search-determinism: the loaded compile failed (exit "
                  f"{r.returncode})\ncmd: {' '.join(cmd)}\n{r.stderr[-2000:]}")
            return 1

        r, t_warm, cmd = compile_once(a.mcc, load_flags, a.source, warm_o,
                                      shared)
        if r.returncode != 0:
            print(f"opt-search-determinism: the warm-cache compile failed "
                  f"(exit {r.returncode})\ncmd: {' '.join(cmd)}\n"
                  f"{r.stderr[-2000:]}")
            return 1

        warmed = sum(len(fs) for _, _, fs in os.walk(shared))
        same_load = filecmp.cmp(idle_o, load_o, shallow=False)
        same_warm = filecmp.cmp(idle_o, warm_o, shallow=False)
        n_idle = os.path.getsize(idle_o)
        n_load = os.path.getsize(load_o)
        n_warm = os.path.getsize(warm_o)
        print(f"opt-search-determinism: {os.path.basename(a.source)} at "
              f"{a.opt}: cold {t_idle:.2f}s -> {n_idle} bytes; pinned to 1 of "
              f"{os.cpu_count()} cpus behind {nload} spinners {t_load:.2f}s -> "
              f"{n_load} bytes (slowdown "
              f"{t_load / max(t_idle, 1e-6):.1f}x); warm cache "
              f"{t_warm:.2f}s -> {n_warm} bytes; {warmed} cache entries "
              f"written under XDG_CACHE_HOME")
        if a.mutate and same_load and same_warm:
            print(f"opt-search-determinism --mutate: the loaded and warm "
                  f"compiles were given -fopt-search-ticks=0 and the cold one "
                  f"the shipped tick count, so the objects genuinely differ, "
                  f"and this tool still reported them byte-identical. A "
                  f"reproducibility gate that passes over objects that differ "
                  f"is comparing a file against itself, and its OK line is a "
                  f"vacuous pass")
            return 0
        if a.mutate:
            print("opt-search-determinism --mutate: the perturbed objects "
                  "differed and the comparison saw it")
            return 1
        if not same_load:
            print(f"FAIL: the object compiled on an unloaded box and the "
                  f"object compiled pinned to one core behind {nload} spinners "
                  f"are not byte-identical ({n_idle} vs {n_load} bytes). "
                  f"{a.opt} is letting elapsed time decide how much of the "
                  f"search space it covers, which is exactly what the tick "
                  f"count replaced. If MCC_SEARCH_CAP_MS is set in this "
                  f"environment, that cap is load-bearing and must be cleared")
            return 1
        if not same_warm:
            print(f"FAIL: the first compile into an empty XDG_CACHE_HOME and "
                  f"the third compile into the cache those runs warmed are not "
                  f"byte-identical ({n_idle} vs {n_warm} bytes). The search "
                  f"memo is being reused across configurations it was not "
                  f"recorded under, so {a.opt} output depends on what the box "
                  f"compiled before it. A tick count fixes the clock; it does "
                  f"not fix a memo key that omits the axis configuration")
            return 1
        print("opt-search-determinism: OK (cold, load-pinned and warm-cache "
              "objects all byte-identical)")
        return 0


if __name__ == "__main__":
    sys.exit(main())
