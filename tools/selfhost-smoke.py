#!/usr/bin/env python3
"""Self-host smoke gate (M8): compile mcc's own source with a built mcc — optionally
with extra MCC_AST_* knobs forced on — link the result into a self-hosted mcc, and
verify that compiler produces correct executables. Catches miscompiles that only
surface on the compiler's own ~100K lines of complex real C.

No dedicated build target exists for this; the recipe is fiddly in two places:
  1. mcc's codegen emits x87 long-double helper calls (__floatundixf/__fixxfdi) that
     GNU ld cannot resolve — LINK WITH mcc ITSELF, whose runtime (runtime/lib/mccrt.c)
     supplies them, not with cc/ld.
  2. the self-hosted mcc needs -I to mcc's bundled freestanding headers (runtime/include).

Usage: tools/selfhost-smoke.py <build-dir> [KNOB=VAL ...]
  tools/selfhost-smoke.py cmake-debug
  tools/selfhost-smoke.py cmake-debug MCC_AST_NARROW_FIX=1 MCC_AST_SETHI_LEAF=1 MCC_AST_SCCP_FIX=1
"""
import json, os, shlex, subprocess, sys, tempfile

def main():
    if len(sys.argv) < 2:
        sys.exit("usage: selfhost-smoke.py <build-dir> [KNOB=VAL ...]")
    bdir = sys.argv[1]
    env = dict(os.environ)
    for kv in sys.argv[2:]:
        k, _, v = kv.partition("=")
        env[k] = v
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    mcc = os.path.join(root, bdir, "mcc")
    blob = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccrt_blob.c.o")
    if not os.access(mcc, os.X_OK):
        sys.exit(f"no mcc at {mcc}")
    if not os.path.exists(blob):
        sys.exit(f"no runtime blob at {blob} (build the mcc target first)")

    cc = json.load(open(os.path.join(root, bdir, "compile_commands.json")))
    rec = [x for x in cc if x["file"].endswith("/mcc.c")][0]
    flags = [a for a in shlex.split(rec["command"])[1:]
             if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]

    with tempfile.TemporaryDirectory() as work:
        print(f"self-host: compiling src/mcc.c with {mcc}  knobs={sys.argv[2:] or '(none)'}")
        obj = os.path.join(work, "mcc-sh.o")
        subprocess.run([mcc, *flags, "-O2", "-c", os.path.join(root, "src/mcc.c"),
                        "-o", obj], cwd=root, env=env, check=True)

        print("self-host: linking (mcc as linker for the x87 long-double helpers)")
        shbin = os.path.join(work, "mcc-sh")
        subprocess.run([mcc, obj, blob, "-o", shbin, "-lm", "-ldl"], cwd=root, check=True)

        inc = os.path.join(root, "runtime/include")
        tc = os.path.join(work, "t.c")
        open(tc, "w").write(
            "#include <stdio.h>\n"
            "int fib(int n){return n<2?n:fib(n-1)+fib(n-2);}\n"
            "int main(void){printf(\"%d\\n\",fib(10));return 0;}\n")
        te = os.path.join(work, "t")
        subprocess.run([shbin, "-I" + inc, tc, "-o", te], check=True)
        out = subprocess.run([te], capture_output=True, text=True).stdout.strip()
        if out != "55":
            sys.exit(f"FAIL: self-hosted mcc gave fib(10)={out}, expected 55")

        qs = os.path.join(root, "tests/exec/programs/quicksort.c")
        if os.path.exists(qs):
            qe = os.path.join(work, "qs")
            subprocess.run([shbin, "-I" + inc, qs, "-o", qe], check=True)
            sortd = subprocess.run([qe], capture_output=True, text=True).stdout.strip().splitlines()[-1]
            if "4 16 21 33 36 37 38 53 55 62 65 74 74 83 89 96" not in sortd:
                sys.exit(f"FAIL: self-hosted mcc mis-sorted quicksort: {sortd}")

    print("self-host: OK (knobs-on compiler self-compiled and produced correct executables)")

if __name__ == "__main__":
    main()
