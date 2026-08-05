#!/usr/bin/env python3
"""Optimizer determinism gate for the item-1 threading work.

Compiles one source file N times with the same mcc + flags + env, and asserts
every object is byte-identical to the first. A parallel optimizer that lets
thread scheduling influence the committed config would produce a diff here; a
correct one (parallel score -> deterministic select -> serial emit) stays
byte-identical across runs.

Usage:
  tools/opt-determinism.py <mcc> <source.c> [-- <extra mcc args...>]
                           [--runs N] [--env KEY=VAL ...]

Example (once the threaded scorer lands):
  tools/opt-determinism.py cmake-build-tsan/mcc_t src/mccstats.c \
      --runs 8 --flag -fopt-search --flag -fopt-search-threads -- -O4 -c
"""
import sys, os, subprocess, tempfile, filecmp, argparse


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
    a = ap.parse_args(argv)

    if not extra:
        extra = ["-O13", "-c"]

    env = dict(os.environ)
    for kv in a.env:
        k, _, v = kv.partition("=")
        env[k] = v

    if not os.path.exists(a.mcc):
        sys.exit(f"determinism: no mcc at {a.mcc}")

    with tempfile.TemporaryDirectory() as work:
        ref = None
        for i in range(a.runs):
            out = os.path.join(work, f"run{i}.o")
            cmd = [a.mcc, *extra, a.source, "-o", out]
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
        print(f"determinism: OK ({a.runs} runs of {os.path.basename(a.source)} "
              f"byte-identical)  flags={' '.join(extra)}  "
              f"env={'+'.join(a.env) or '(none)'}")


if __name__ == "__main__":
    main()
