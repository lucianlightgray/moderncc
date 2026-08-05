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
import json, os, re, shlex, subprocess, sys, tempfile

def main():
    if len(sys.argv) < 2:
        sys.exit("usage: selfhost-smoke.py <build-dir> [-fknob ...]")
    bdir = sys.argv[1]
    env = dict(os.environ)
    # Knobs arrive as compiler flags and go on the command line. They used to be
    # KNOB=VAL pairs stuffed into the environment; the compiler no longer reads
    # any of those, so a pair here would be silently ignored rather than fail.
    knobs = list(sys.argv[2:])
    bad = [k for k in knobs if not k.startswith("-")]
    if bad:
        sys.exit("selfhost-smoke: knobs must be compiler flags, got: " + " ".join(bad))
    if os.name == "nt":
        try:
            import ctypes
            ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
        except Exception:
            pass
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    mcc = os.path.join(root, bdir, "mcc")
    if not os.access(mcc, os.X_OK) and os.access(mcc + ".exe", os.X_OK):
        mcc += ".exe"
    pe = mcc.endswith(".exe")
    blob = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccrt_blob.c.o")
    sidecars = [os.path.join(root, bdir, "libmccrt.a"),
                os.path.join(root, bdir, "lib", "libmccrt.a")]
    if not os.access(mcc, os.X_OK):
        sys.exit(f"no mcc at {mcc}")

    cc = json.load(open(os.path.join(root, bdir, "compile_commands.json")))
    rec = [x for x in cc if x["file"].endswith("/mcc.c")][0]
    cmd = rec["command"]
    if os.name == "nt":
        cmd = re.sub(r'\\(?!")', r'\\\\', cmd)
    flags = [a for a in shlex.split(cmd)[1:]
             if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]

    link_objs = []
    link_flags = []
    if os.path.exists(blob):
        link_objs.append(blob)
    elif any(os.path.exists(p) for p in sidecars):
        link_flags += ["-B", os.path.dirname(next(p for p in sidecars if os.path.exists(p)))]
    else:
        sys.exit(f"no runtime blob at {blob} and no sidecar libmccrt.a in "
                 f"{os.path.join(root, bdir)} (build the mcc target first)")
    if any(a.startswith("-DMCC_EMBED_JIT_BLOB") for a in flags):
        jbase = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c")
        jitblob = next((jbase + ext for ext in (".o", ".obj")
                        if os.path.exists(jbase + ext)), None)
        if not jitblob:
            sys.exit(f"no JIT blob at {jbase}.o[bj] (build the mcc target first)")
        link_objs.append(jitblob)

    win32_pre = ["-B", os.path.join(root, "runtime/win32")] if pe else []
    link_libs = [] if pe else ["-lm", "-ldl"]
    exe = ".exe" if pe else ""

    with tempfile.TemporaryDirectory() as work:
        print(f"self-host: compiling src/mcc.c with {mcc}  knobs={knobs or '(none)'}")
        obj = os.path.join(work, "mcc-sh.o")
        subprocess.run([mcc, *flags, "-O2", *knobs, "-c",
                        os.path.join(root, "src/mcc.c"),
                        "-o", obj], cwd=root, env=env, check=True)

        print("self-host: linking (mcc as linker for the x87 long-double helpers)")
        shbin = os.path.join(work, "mcc-sh" + exe)
        subprocess.run([mcc, *link_flags, *win32_pre, obj, *link_objs, "-o", shbin,
                        *link_libs], cwd=root, check=True)

        inc = os.path.join(root, "runtime/include")
        tc = os.path.join(work, "t.c")
        open(tc, "w").write(
            "#include <stdio.h>\n"
            "int fib(int n){return n<2?n:fib(n-1)+fib(n-2);}\n"
            "int main(void){printf(\"%d\\n\",fib(10));return 0;}\n")
        te = os.path.join(work, "t" + exe)
        try:
            subprocess.run([shbin, *link_flags, *win32_pre, "-I" + inc, tc, "-o", te], check=True)
        except subprocess.CalledProcessError as e:
            print(f"self-host: shbin compile FAILED (exit {e.returncode}); classifying", flush=True)
            ver = subprocess.run([shbin, "--version"], capture_output=True, text=True)
            print(f"self-host:   shbin --version -> exit {ver.returncode} "
                  f"out={(ver.stdout or ver.stderr).strip()[:120]!r}", flush=True)
            nojit_env = dict(os.environ, MCC_JIT="0")
            vnj = subprocess.run([shbin, "--version"], capture_output=True, text=True, env=nojit_env)
            print(f"self-host:   shbin --version MCC_JIT=0 -> exit {vnj.returncode} "
                  f"out={(vnj.stdout or vnj.stderr).strip()[:120]!r}", flush=True)
            ec = os.path.join(work, "empty.c")
            ee = os.path.join(work, "empty" + exe)
            open(ec, "w").write("int main(void){return 0;}\n")
            noinc = subprocess.run([shbin, *link_flags, *win32_pre, ec, "-o", ee])
            print(f"self-host:   shbin compile (no #include) -> exit {noinc.returncode}", flush=True)
            raise
        out = subprocess.run([te], capture_output=True, text=True).stdout.strip()
        if out != "55":
            sys.exit(f"FAIL: self-hosted mcc gave fib(10)={out}, expected 55")

        qs = os.path.join(root, "tests/exec/programs/quicksort.c")
        if os.path.exists(qs):
            qe = os.path.join(work, "qs" + exe)
            subprocess.run([shbin, *link_flags, *win32_pre, "-I" + inc, qs, "-o", qe], check=True)
            sortd = subprocess.run([qe], capture_output=True, text=True).stdout.strip().splitlines()[-1]
            if "4 16 21 33 36 37 38 53 55 62 65 74 74 83 89 96" not in sortd:
                sys.exit(f"FAIL: self-hosted mcc mis-sorted quicksort: {sortd}")

    print("self-host: OK (knobs-on compiler self-compiled and produced correct executables)")

if __name__ == "__main__":
    main()
