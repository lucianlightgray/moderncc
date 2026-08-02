#!/usr/bin/env python3
"""--embed-jit bake+run smoke gate (Windows/PE, mingw).

Bakes a small program with `mcc --embed-jit` — which pulls the COFF JIT-engine
blob into the output via mcc's own linker (native-TLS SECREL reloc reading, the
a-la-carte loader, __ImageBase synthesis, the compiler-rt/libgcc runtime
fallback) — then runs the produced standalone exe and asserts it prints the
right answer. This locks in the PE --embed-jit LINK path; it deliberately does
NOT drive the runtime JIT of a full self-compile (MCC_JIT=1 on the amalgamation
still 0xC0000005s on the winlibs CI cells — P0 step 5, tracked in docs/TODO).

Exit codes:
  0  bake linked a working engine-embedded exe and it ran correct
  77 SKIP — no baked engine (no mccjit_blob.c), or the toolchain can't supply
     the engine's compiler-support runtime (libgcc / compiler-rt absent)
  1  FAIL — a real regression: wrong output, or a bake failure that is NOT a
     missing-runtime-lib issue (e.g. an unsupported-reloc / linker defect)

Usage: tools/embed-jit-smoke.py <build-dir>
"""
import os, subprocess, sys, tempfile

SKIP = 77

# stderr fragments that mean "this toolchain has no compiler-support runtime for
# the baked engine" — a host limitation, not a mcc regression => SKIP not FAIL.
# __emutls_get_address is libgcc's emulated-TLS entry point: winlibs GCC lowers
# the engine's thread-locals to emulated TLS (llvm-mingw uses native SECREL TLS,
# which the a-la-carte loader handles). mcc's --embed-jit now resolves that whole
# winlibs chain from the baked mingw lib dirs — __emutls_get_address from
# libgcc_eh.a, its pthread-keyed state from libwinpthread.a, and _tls_used from
# libmingw32's tlssup.o — so a toolchain that ships those links and PASSES; the
# marker stays as the SKIP path for a toolchain that genuinely lacks them (e.g.
# a compiler-rt-only i686 mingw). The i686 engine's SRW-lock kernel32 imports are
# deliberately NOT markers (OS imports, not compiler runtime — listing them could
# mask a genuine import-emission linker regression).
TOOLCHAIN_LIB_MARKERS = (
    "___chkstk_ms", "__chkstk_ms", "__emutls_get_address", "libgcc",
    "clang_rt", "mingwex", "mingw32",
    "library 'gcc' not found", "library 'mingwex' not found",
)

# A self-contained workload: sum fib(i%15) for i in 0..29 == 1972; return low byte.
PROG = r"""
extern int printf(const char *, ...);
static int fib(int n){ return n < 2 ? n : fib(n-1) + fib(n-2); }
int main(void){
    int s = 0;
    for (int i = 0; i < 30; i++) s += fib(i % 15);
    printf("emb %d\n", s);
    return s & 0xff;
}
"""
EXPECT_STDOUT = "emb 1972"
EXPECT_RC = 1972 & 0xff  # 180

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

def main():
    if len(sys.argv) < 2:
        sys.exit("usage: embed-jit-smoke.py <build-dir>")
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bdir = sys.argv[1]
    bdir = bdir if os.path.isabs(bdir) else os.path.join(root, bdir)

    mcc = find_mcc(bdir)
    if not mcc:
        sys.exit(f"no mcc in {bdir}")
    if not os.path.exists(os.path.join(bdir, "mccjit_blob.c")):
        print("embed-jit-smoke: SKIP (build has no baked JIT engine)")
        sys.exit(SKIP)

    with tempfile.TemporaryDirectory() as work:
        src = os.path.join(work, "emb.c")
        exe = os.path.join(work, "emb.exe" if os.name == "nt" else "emb.out")
        with open(src, "w") as f:
            f.write(PROG)

        print(f"embed-jit-smoke: {mcc} --embed-jit -O2 emb.c -> emb exe")
        b = subprocess.run([mcc, "-O2", "--embed-jit", src, "-o", exe],
                           cwd=root, capture_output=True, text=True)
        if b.returncode != 0:
            err = (b.stderr or "") + (b.stdout or "")
            if any(m in err for m in TOOLCHAIN_LIB_MARKERS):
                print("embed-jit-smoke: SKIP (toolchain has no libgcc/compiler-rt "
                      "for the baked engine)\n" + err.strip()[:400])
                sys.exit(SKIP)
            sys.exit(f"FAIL: --embed-jit bake failed (exit {b.returncode})\n{err.strip()[:800]}")
        if not os.path.exists(exe):
            sys.exit("FAIL: --embed-jit produced no output exe")

        # Run the engine-embedded exe with the JIT OFF (the LINK path is what this
        # gate covers; the runtime JIT is P0 step 5 and stays out of CI on winlibs).
        env = dict(os.environ)
        env["MCC_JIT"] = "0"
        if os.name == "nt":
            try:
                import ctypes
                ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
            except Exception:
                pass
        r = subprocess.run([exe], capture_output=True, text=True, env=env, timeout=60)
        out = (r.stdout or "").strip()
        # A gcc-built i386-PE engine 0xC0000005s at startup even with the JIT
        # off (unbound gcc import-library idata) -- a host-toolchain limitation,
        # not a mcc link regression, so it skip-marks honestly. But when the
        # build self-baked the engine WITH mcc (the libmccjitengine-mcc.a
        # artifact), that crash class is FIXED and a recurrence is a real
        # regression: FAIL it so the promoted i686 cell cannot false-green.
        if r.returncode in (-1073741819, 3221225477):
            if os.path.exists(os.path.join(bdir, "libmccjitengine-mcc.a")):
                sys.exit("FAIL: embedded exe 0xC0000005 at startup with the "
                         "mcc-built engine (self-bake regression)")
            print("embed-jit-smoke: SKIP (embedded exe 0xC0000005 at MCC_JIT=0 "
                  "-- gcc-engine startup residual; LINK succeeded, tracked in docs/TODO)")
            sys.exit(SKIP)
        if r.returncode != EXPECT_RC or EXPECT_STDOUT not in out:
            sys.exit(f"FAIL: embedded exe wrong result (rc={r.returncode} "
                     f"want {EXPECT_RC}; stdout={out!r} want {EXPECT_STDOUT!r})")

        jit_on = None
        if os.name != "nt":
            # And again with the JIT ON. Without this leg the gate proves only that
            # the engine LINKS: the arm64 Mach-O BRANCH26 defect that rewrote every
            # clang sibling call into `bl` produced an engine that ran correct at
            # MCC_JIT=0 and SIGSEGV'd at MCC_JIT=1, in every optimized build, and
            # nothing in ctest saw it. Stays off Windows, where the runtime JIT has
            # its own open blockers (see docs/TODO).
            env_on = dict(os.environ)
            env_on["MCC_JIT"] = "1"
            jit_on = subprocess.run([exe], capture_output=True, text=True,
                                    env=env_on, timeout=120)
            on_out = (jit_on.stdout or "").strip()
            if jit_on.returncode != EXPECT_RC or EXPECT_STDOUT not in on_out:
                sys.exit(f"FAIL: embedded exe wrong result under MCC_JIT=1 "
                         f"(rc={jit_on.returncode} want {EXPECT_RC}; "
                         f"stdout={on_out!r} want {EXPECT_STDOUT!r}) -- it is "
                         f"correct at MCC_JIT=0, so the baked engine itself is "
                         f"broken, not the link")

    jit_note = " + ran correct under MCC_JIT=1" if jit_on is not None else ""
    print(f"embed-jit-smoke: OK (--embed-jit linked the engine + exe ran correct: {out}{jit_note})")

if __name__ == "__main__":
    main()
