#!/usr/bin/env python3
"""Three-way differential for the wide-integer / _BitInt test corpus.

The exec/dt cells compare mcc against a STORED, target-independent golden, which
proves nothing about whether mcc agrees with a real compiler. This compiles each
sectioned test (#if defined test_X ... #elif ...) with mcc, gcc AND clang, runs
all three, and compares their output line-by-line, so a value that mcc gets
wrong -- or that gcc and clang disagree on -- is caught against live compilers.

VALUE parity is a hard gate: for every non-ABI line, mcc must match BOTH gcc and
clang (which must also match each other, else the line is UB/impl-defined and
skipped). ABI lines (size/align/maxwidth) are reported separately, because mcc
deliberately uses a uniform _BitInt storage ABI that differs from native gcc/clang
on some targets -- that divergence is expected and flagged, not failed.

Usage:
  tools/bitint-diff.py [--mcc PATH] [--bdir DIR] FILE.c [FILE.c ...]
Exit: 0 all value lines agree, 1 a real value divergence, 77 no compilers.
"""
import argparse, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ABI_RE = re.compile(r'\b(size|sizeof|align|alignof|maxwidth|sz|off)\b', re.I)


def sections(src):
    """Every test_X selected by an `#if/#elif defined test_X`."""
    return sorted(set(re.findall(r'defined\s+(test_\w+)', open(src).read())))


def run(cmd, **kw):
    p = subprocess.run(cmd, capture_output=True, text=True, **kw)
    return p.returncode, p.stdout, p.stderr


def build_run(compiler, src, sect, td, mcc, bdir):
    """Compile `src` with -Dsect and run; return (ok, output) or (False, err)."""
    if compiler == "mcc":
        rc, out, err = run([mcc, "-B", bdir, "-D" + sect, "-run", src])
        return (rc == 0, out if rc == 0 else err[:200])
    exe = os.path.join(td, compiler + "_" + sect)
    rc, _, err = run([compiler, "-std=c23", "-w", "-D" + sect, src, "-o", exe, "-lm"])
    if rc != 0:
        return (False, "build: " + err.strip()[:200])
    rc, out, err = run([exe])
    return (rc == 0, out if rc == 0 else "run rc=%d" % rc)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mcc", default=os.path.join(ROOT, "cmake-debug", "mcc"))
    ap.add_argument("--bdir", default=os.path.join(ROOT, "cmake-debug"))
    ap.add_argument("--min-lines", type=int, default=1,
                    help="fail if fewer than this many value lines were actually "
                         "compared against gcc+clang (floor against a vacuous pass)")
    ap.add_argument("files", nargs="+")
    a = ap.parse_args()

    import shutil
    gcc = shutil.which("gcc")
    clang = shutil.which("clang")
    if not gcc or not clang:
        print("need both gcc and clang for the 3-way differential; skipping")
        return 77
    if not os.access(a.mcc, os.X_OK):
        print("no mcc at %s; skipping" % a.mcc)
        return 77

    value_fail = abi_div = value_ok = ref_ambig = 0
    with tempfile.TemporaryDirectory() as td:
        for src in a.files:
            name = os.path.basename(src)
            for sect in sections(src):
                res = {c: build_run(c, src, sect, td, a.mcc, a.bdir)
                       for c in ("gcc", "clang", "mcc")}
                if not res["gcc"][0] or not res["clang"][0]:
                    print("SKIP %s/%s: reference build failed (%s)" %
                          (name, sect, (res["gcc"][1] or res["clang"][1])[:80]))
                    continue
                if not res["mcc"][0]:
                    print("FAIL %s/%s: mcc failed: %s" % (name, sect, res["mcc"][1][:120]))
                    value_fail += 1
                    continue
                g = res["gcc"][1].splitlines()
                c = res["clang"][1].splitlines()
                m = res["mcc"][1].splitlines()
                n = max(len(g), len(c), len(m))
                for i in range(n):
                    lg = g[i] if i < len(g) else "<none>"
                    lc = c[i] if i < len(c) else "<none>"
                    lm = m[i] if i < len(m) else "<none>"
                    is_abi = ABI_RE.search(lg) or ABI_RE.search(lm)
                    if lg != lc:
                        ref_ambig += 1            # gcc vs clang disagree -> UB/impl-def
                        continue
                    if lm == lg:
                        value_ok += 1
                        continue
                    if is_abi:
                        abi_div += 1
                        print("  ABI  %s/%s: mcc %r vs gcc/clang %r" % (name, sect, lm, lg))
                    else:
                        value_fail += 1
                        print("FAIL %s/%s: mcc %r != gcc==clang %r" % (name, sect, lm, lg))

    print("\nbitint-diff: %d value lines agree with gcc+clang, %d ABI divergences "
          "(mcc uniform ABI, expected), %d gcc/clang-ambiguous skipped, %d VALUE FAILURES"
          % (value_ok, abi_div, ref_ambig, value_fail))
    if value_fail:
        return 1
    if value_ok < a.min_lines:
        print("bitint-diff: VACUOUS — only %d value lines were actually compared "
              "against gcc+clang (floor %d). With no value lines the gate proves "
              "nothing (references failed to build, or the corpus lost its test_X "
              "value sections), so it fails rather than passing silently."
              % (value_ok, a.min_lines))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
