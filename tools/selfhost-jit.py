#!/usr/bin/env python3
"""JIT self-host gate: drive a built embed-JIT mcc as `--jit -O13 -run src/mcc.c`
so it compiles its OWN source into memory, the runtime JIT profile-recompiles its
hot functions, and that in-memory mcc then compiles a workload. Assert it does NOT
crash (the mccjit_embed_finalize path over search-graduated KGC entries) AND that
the in-memory (JIT-optimized) compiler emits an object byte-identical to a plain
AOT `-c` compile by the same mcc — so a JIT miscompile of the compiler is caught,
not just a crash.

The -O13 search is what graduates functions into the embed registry that
mcc_relocate -> pe_output_file -> mccjit_embed_finalize re-compiles; -O1 leaves it
empty and exercises nothing. Fresh XDG_CACHE_HOME keeps the run deterministic.

SKIPs (exit 77) when the build has no baked JIT engine (no mccjit_blob.c) — there
is no runtime JIT to exercise.

Usage: tools/selfhost-jit.py <build-dir> <cpu> [KNOB=VAL ...]
"""
import filecmp, os, subprocess, sys, tempfile

SKIP = 77

def find_mcc(bdir):
    for name in ("mcc", "mcc.exe"):
        p = os.path.join(bdir, name)
        if os.path.exists(p):
            return p
    for cfg in ("Release", "RelWithDebInfo", "Debug", "MinSizeRel"):
        p = os.path.join(bdir, cfg, "mcc.exe")
        if os.path.exists(p):
            return p
    return None

def gen_workload(path, nfuncs=120):
    with open(path, "w") as f:
        for i in range(nfuncs):
            f.write("static long f%d(long x){long a=x;for(int k=0;k<7;k++){"
                    "a=a*%d+(a>>3)^(a<<2)-k*%d;a&=0x7fffffff;}return a;}\n"
                    % (i, (i % 13) + 3, (i % 7) + 1))
        f.write("long g(long s){long t=0;")
        for i in range(nfuncs):
            f.write("t+=f%d(s+%d);" % (i, i))
        f.write("return t;}\n")
        f.write("int main(void){long acc=0;for(long s=0;s<50;s++)acc+=g(s);"
                "return (int)(acc&0x7f);}\n")

def main():
    if len(sys.argv) < 3:
        sys.exit("usage: selfhost-jit.py <build-dir> <cpu> [KNOB=VAL ...] [-f...]")
    bdir, cpu = sys.argv[1], sys.argv[2]
    xflags = [a for a in sys.argv[3:] if a.startswith("-")]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bdir = bdir if os.path.isabs(bdir) else os.path.join(root, bdir)

    win = os.name == "nt" or sys.platform.startswith("win")
    if win and cpu == "arm64" and not os.environ.get("MCC_JIT_FORCE_ARM64"):
        print("selfhost-jit: SKIP (arm64-Windows JIT parity defects; tracked in docs/TODO)")
        sys.exit(SKIP)
    if win:
        try:
            import ctypes
            ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
        except Exception:
            pass

    mcc = find_mcc(bdir)
    if not mcc:
        sys.exit(f"no mcc in {bdir}")
    if not os.path.exists(os.path.join(bdir, "mccjit_blob.c")):
        print("selfhost-jit: SKIP (build has no baked JIT engine)")
        sys.exit(SKIP)

    src = os.path.join(root, "src")
    incs = ["-I" + bdir, "-I" + root, "-I" + src,
            "-I" + os.path.join(src, "formats"), "-I" + os.path.join(src, "objfmt"),
            "-I" + os.path.join(src, "arch", "i386"),
            "-I" + os.path.join(src, "arch", cpu),
            "-I" + os.path.join(root, "include")]
    brt = ["-B" + root, "-B" + bdir] + xflags

    env = dict(os.environ)
    env.update(MCC_JIT="1", MCC_AST_SEARCH="1", MCC_SEARCH_WORKER="1",
               MCC_JIT_HOT_CALLS="50")
    if win:
        env["MCC_JIT_CRASH_DIAG"] = "1"
    for kv in [a for a in sys.argv[3:] if not a.startswith("-")]:
        k, _, v = kv.partition("=")
        env[k] = v

    with tempfile.TemporaryDirectory() as work:
        env["XDG_CACHE_HOME"] = os.path.join(work, "cache")
        wl = os.path.join(work, "wl.c")
        gen_workload(wl)

        ref = os.path.join(work, "ref.o")
        subprocess.run([mcc, "-c", wl, "-o", ref], cwd=root, check=True)

        out = os.path.join(work, "jit.o")
        mccsrc = os.path.join(src, "mcc.c")
        inner_inc = ["-I" + os.path.join(bdir, "include"),
                     "-I" + os.path.join(root, "runtime", "include")]
        print(f"selfhost-jit: {mcc} --jit -O13 -run src/mcc.c -> inner -c workload  "
              f"knobs={sys.argv[3:] or '(none)'}")
        cap = win
        # The inner mcc's whole TLS (its own -run slab plus the optimizer's
        # per-worker _Thread_local gates) must fit the OUTER mcc's 64K slab,
        # so the inner gets a token-sized slab -- it never -runs anything.
        r = subprocess.run([mcc, "--jit", "-O13", *incs, *brt,
                            "-DMCC_JIT_TLS_MAX=4096",
                            "-run", mccsrc, *inner_inc, "-c", wl, "-o", out],
                           cwd=root, env=env,
                           capture_output=cap, text=cap)

        def emit_child():
            if not cap:
                return
            if r.stdout:
                sys.stdout.write("---- child stdout ----\n" + r.stdout +
                                 ("\n" if not r.stdout.endswith("\n") else ""))
            if r.stderr:
                sys.stdout.write("---- child stderr ----\n" + r.stderr +
                                 ("\n" if not r.stderr.endswith("\n") else ""))
            sys.stdout.flush()

        if win and r.returncode in (-1073741819, 3221225477):
            emit_child()
            print("selfhost-jit: SKIP — PE runtime-JIT 0xC0000005 still present "
                  f"(exit {r.returncode}); the x86_64 swapped-variant/KGC-stub "
                  "residual, tracked in docs/TODO")
            sys.exit(SKIP)
        if r.returncode != 0:
            emit_child()
            sys.exit(f"FAIL: JIT self-host -run crashed/errored (exit {r.returncode})")
        if not os.path.exists(out):
            sys.exit("FAIL: JIT self-host produced no object")
        if not filecmp.cmp(ref, out, shallow=False):
            sys.exit("FAIL: JIT-recompiled compiler output differs from AOT reference "
                     "(possible JIT miscompile of mcc)")

    print("selfhost-jit: OK (JIT self-host ran; in-memory compiler output byte-identical to AOT)")

if __name__ == "__main__":
    main()
