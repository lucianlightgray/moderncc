#!/usr/bin/env python3
"""Optimizer determinism gate for the item-1 threading work.

Compiles one source file N times with the same mcc + flags + env, and asserts
every object is byte-identical to the first. A parallel optimizer that lets
thread scheduling influence the committed config would produce a diff here; a
correct one (parallel score -> deterministic select -> serial emit) stays
byte-identical across runs.

Usage:
  tools/opt-determinism.py <mcc> <source.c> [-- <extra mcc args...>]
                           [--runs N] [--env KEY=VAL ...] [--from-build DIR]
                           [--mutate]

Exit status: 0 byte-identical, 1 a run differed or would not build, 77 skip
(no runnable mcc, no source, no compile_commands.json for --from-build).

Example (once the threaded scorer lands):
  tools/opt-determinism.py cmake-build-tsan/mcc_t src/mccstats.c \
      --runs 8 --flag -fopt-search --flag -fopt-search-threads -- -O4 -c
"""
import sys, os, json, shlex, subprocess, tempfile, filecmp, argparse


def main():
    argv = sys.argv[1:]
    extra = []
    if "--" in argv:
        i = argv.index("--")
        argv, extra = argv[:i], argv[i + 1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("mcc")
    ap.add_argument("source")
    ap.add_argument("--runs", type=int, default=8)
    ap.add_argument("--env", action="append", default=[],
                    help="KEY=VAL env override (repeatable)")
    ap.add_argument("--mutate", action="store_true",
                    help="the known-positive arm: compile run 0 at -O0 while "
                         "the rest use the given flags, so the objects really "
                         "do differ and this tool must say so. Without it a "
                         "run that compared a file against itself, or that "
                         "never opened the objects at all, would be "
                         "indistinguishable from a deterministic optimizer")
    ap.add_argument("--from-build", default=None,
                    help="take the -D/-I set for src/mcc.c straight out of that "
                         "build tree's compile_commands.json, so the subject is "
                         "the program the build compiles and not a guessed "
                         "approximation of it")
    a = ap.parse_args(argv)

    if not extra:
        extra = ["-O13", "-c"]

    if a.from_build:
        cdb = os.path.join(a.from_build, "compile_commands.json")
        if not os.path.exists(cdb):
            print(f"determinism: no compile_commands.json in {a.from_build}; "
                  f"skipping")
            sys.exit(77)
        recs = [x for x in json.load(open(cdb)) if x["file"].endswith("/mcc.c")]
        if not recs:
            print(f"determinism: no src/mcc.c record in {cdb}; skipping")
            sys.exit(77)
        extra = [x for x in shlex.split(recs[0]["command"])[1:]
                 if (x.startswith("-D") or x.startswith("-I"))
                 and not x.endswith(".c")] + extra

    env = dict(os.environ)
    for kv in a.env:
        k, _, v = kv.partition("=")
        env[k] = v

    if not os.access(a.mcc, os.X_OK):
        print(f"determinism: no runnable mcc at {a.mcc}; skipping")
        sys.exit(77)
    if not os.path.exists(a.source):
        print(f"determinism: no source at {a.source}; skipping")
        sys.exit(77)
    if a.runs < 2:
        sys.exit(f"determinism: --runs {a.runs} compares nothing against "
                 f"anything; the OK line below would report byte-identity over "
                 f"a single object, or over none")

    with tempfile.TemporaryDirectory() as work:
        ref = None
        for i in range(a.runs):
            out = os.path.join(work, f"run{i}.o")
            flags = list(extra)
            if a.mutate and i == 0:
                flags = [("-O0" if f.startswith("-O") else f) for f in flags]
                if not any(f.startswith("-O") for f in flags):
                    flags.append("-O0")
            cmd = [a.mcc, *flags, a.source, "-o", out]
            r = subprocess.run(cmd, env=env, capture_output=True, text=True)
            if r.returncode != 0:
                sys.exit(f"determinism: run {i} failed (exit {r.returncode})\n"
                         f"cmd: {' '.join(cmd)}\n{r.stderr[-2000:]}")
            if not os.path.exists(out):
                sys.exit(f"determinism: run {i} produced no object")
            if ref is None:
                ref = out
            elif not filecmp.cmp(ref, out, shallow=False):
                sys.exit(f"FAIL: run {i} object differs from run 0 "
                         f"(non-deterministic optimizer output)")
        if a.mutate:
            print(f"determinism --mutate: run 0 was compiled at -O0 and the "
                  f"other {a.runs - 1} at {' '.join(extra)}, and all "
                  f"{a.runs} objects came out byte-identical. Either the -O "
                  f"level changes nothing about {os.path.basename(a.source)} "
                  f"or this tool is not comparing the objects it wrote")
            return
        print(f"determinism: OK ({a.runs} runs of {os.path.basename(a.source)} "
              f"byte-identical)  flags={' '.join(extra)}  "
              f"env={'+'.join(a.env) or '(none)'}")


if __name__ == "__main__":
    main()
