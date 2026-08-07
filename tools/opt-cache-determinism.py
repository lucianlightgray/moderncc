#!/usr/bin/env python3
"""Codegen must not depend on what is already in the optimizer's disk cache.

tools/opt-determinism.py compiles the same file N times in the same
environment, which cannot see this class of defect at all: after the first run
every later run shares one warm cache, so they agree with each other and the
gate passes. The question that matters to a user is different -- does the
object I get depend on what my cache happens to hold? Two developers on the
same commit, one with a cache warmed by other work, must get the same bytes.

Four cache states, one source, one set of flags:

  cold        an empty XDG_CACHE_HOME
  self        an empty one, compiled twice; the second run sees its OWN entries
  foreign-tu  one pre-warmed by compiling a DIFFERENT translation unit
  foreign-fl  one pre-warmed by compiling THIS source at a different flag set

All four objects must be byte-identical. The last two are the states the first
two cannot reach, and they are where this bites: the slice memo lives in
sl-<target-salt>.ck, salted by the ISA rather than by what was compiled, so
records written under one configuration are read back under another. Measured
on this tree: src/mcc.c at -O3 emits 3401167 bytes from a cold cache and
3391663 from one warmed by the same file at -O3 -fno-reemit-templates. -O3
-fno-opt-slice is identical in every state, which is what identifies the pass.

Usage:
  tools/opt-cache-determinism.py <mcc> <source.c> [--other <other.c>]
      [--opt=-O3] [--from-build <dir>] [-- <extra mcc args...>]

  --from-build takes the -D/-I set for src/mcc.c straight out of that build
  tree's compile_commands.json. The defect is only reachable with the real
  configuration -- a hand-typed include list leaves out the defines that select
  the code the slice pass memoises -- and a shell cannot pass those defines
  through word splitting intact anyway.

Exit status: 0 identical, 1 the cache changed the object, 77 skip.
"""
import argparse, hashlib, json, os, shlex, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sha(path):
    with open(path, "rb") as fh:
        return hashlib.sha1(fh.read()).hexdigest()


def compile_into(mcc, src, obj, cache, extra, opt):
    env = dict(os.environ)
    env["XDG_CACHE_HOME"] = cache
    os.makedirs(cache, exist_ok=True)
    argv = [mcc, opt] + extra + ["-c", src, "-o", obj]
    p = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, env=env)
    return p


def main():
    argv = sys.argv[1:]
    extra = []
    if "--" in argv:
        i = argv.index("--")
        argv, extra = argv[:i], argv[i + 1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("mcc")
    ap.add_argument("source")
    ap.add_argument("--other", default=None)
    ap.add_argument("--opt", default="-O3")
    ap.add_argument("--from-build", default=None)
    ap.add_argument("--warm-flag", action="append", default=None)
    a = ap.parse_args(argv)
    warm_flags = a.warm_flag if a.warm_flag is not None else ["-fno-reemit-templates"]

    if a.from_build:
        cdb = os.path.join(a.from_build, "compile_commands.json")
        if not os.path.exists(cdb):
            print("no compile_commands.json in %s; skipping" % a.from_build)
            return 77
        recs = [x for x in json.load(open(cdb)) if x["file"].endswith("/mcc.c")]
        if not recs:
            print("no src/mcc.c record in %s; skipping" % cdb)
            return 77
        extra = [x for x in shlex.split(recs[0]["command"])[1:]
                 if (x.startswith("-D") or x.startswith("-I"))
                 and not x.endswith(".c")] + extra

    if not os.access(a.mcc, os.X_OK):
        print("no mcc at %s; skipping" % a.mcc)
        return 77
    if not os.path.exists(a.source):
        print("no source at %s; skipping" % a.source)
        return 77
    other = a.other
    if not other:
        for cand in ("src/libmcc.c", "src/mcctools.c", "src/mccstats.c"):
            p = os.path.join(ROOT, cand)
            if os.path.exists(p) and os.path.abspath(p) != os.path.abspath(a.source):
                other = p
                break
    if not other or not os.path.exists(other):
        print("no second translation unit to warm the cache with; skipping")
        return 77

    work = tempfile.mkdtemp(prefix="cachedet-")
    try:
        shas = {}
        cold = os.path.join(work, "c-cold")
        obj = os.path.join(work, "cold.o")
        p = compile_into(a.mcc, a.source, obj, cold, extra, a.opt)
        if p.returncode != 0:
            print("FAIL cold compile: %s" % p.stderr.strip()[-400:])
            return 1
        shas["cold"] = sha(obj)

        selfd = os.path.join(work, "c-self")
        o1 = os.path.join(work, "self1.o")
        o2 = os.path.join(work, "self2.o")
        compile_into(a.mcc, a.source, o1, selfd, extra, a.opt)
        p = compile_into(a.mcc, a.source, o2, selfd, extra, a.opt)
        if p.returncode != 0:
            print("FAIL self-warm compile: %s" % p.stderr.strip()[-400:])
            return 1
        shas["self"] = sha(o2)

        fdir = os.path.join(work, "c-foreign-tu")
        warm = os.path.join(work, "warm.o")
        p = compile_into(a.mcc, other, warm, fdir, extra, a.opt)
        if p.returncode != 0:
            print("SKIP: could not compile %s to warm the cache: %s"
                  % (other, p.stderr.strip()[-200:]))
            return 77
        o3 = os.path.join(work, "foreign-tu.o")
        p = compile_into(a.mcc, a.source, o3, fdir, extra, a.opt)
        if p.returncode != 0:
            print("FAIL foreign-tu compile: %s" % p.stderr.strip()[-400:])
            return 1
        shas["foreign-tu"] = sha(o3)

        gdir = os.path.join(work, "c-foreign-fl")
        o4 = os.path.join(work, "warm-fl.o")
        p = compile_into(a.mcc, a.source, o4, gdir, extra + warm_flags, a.opt)
        if p.returncode != 0:
            print("SKIP: could not warm with %s: %s"
                  % (" ".join(warm_flags), p.stderr.strip()[-200:]))
            return 77
        o5 = os.path.join(work, "foreign-fl.o")
        p = compile_into(a.mcc, a.source, o5, gdir, extra, a.opt)
        if p.returncode != 0:
            print("FAIL foreign-fl compile: %s" % p.stderr.strip()[-400:])
            return 1
        shas["foreign-fl"] = sha(o5)

        order = ("cold", "self", "foreign-tu", "foreign-fl")
        for k in order:
            print("%-11s %s" % (k, shas[k]))
        if len(set(shas.values())) == 1:
            print("opt-cache-determinism: OK (%s %s: the disk cache is a side "
                  "channel, not an input to codegen)"
                  % (os.path.basename(a.source), a.opt))
            return 0
        print("FAIL: the object depends on the optimizer's disk cache state "
              "(%s %s).\n"
              "      %s\n"
              "      Two builds of the same source at the same flags disagree "
              "because one machine's cache held records written under a "
              "different configuration. The cache warmed for foreign-fl was "
              "written by: %s"
              % (os.path.basename(a.source), a.opt,
                 "  ".join("%s=%s" % (k, shas[k][:12]) for k in order),
                 " ".join([a.opt] + warm_flags)))
        return 1
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
