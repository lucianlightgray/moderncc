#!/usr/bin/env python3
"""3-stage self-host fixpoint gate (M8): mcc must reproduce itself byte-for-byte.

  stage1: the built mcc (stage0, host-cc-built) compiles src/mcc.c -> o1, link -> mcc1
  stage2: mcc1 compiles src/mcc.c -> o2, link -> mcc2
  stage3: mcc2 compiles src/mcc.c -> o3
assert o2 == o3 (self-host fixpoint) and o1 == o2 (host-cc- and mcc-built mcc emit
identical code). Any drift means nondeterministic or unstable self-host codegen.

Shares the selfhost-smoke recipe: link WITH mcc (its runtime supplies the x87
long-double helpers GNU ld can't), and add the embedded JIT blob object when the
build bakes it (MCC_EMBED_JIT_BLOB).

Usage: tools/selfhost-fixpoint.py <build-dir> [KNOB=VAL ...]
"""
import json, os, re, shlex, subprocess, sys, tempfile

def main():
    if len(sys.argv) < 2:
        sys.exit("usage: selfhost-fixpoint.py <build-dir> [--opt=-ON] [KNOB=VAL ...]")
    bdir = sys.argv[1]
    env = dict(os.environ)
    # --opt=<level> picks the -O level under test. Defaults to -O2 for
    # compatibility, but -O2 alone never exercises the -O3 defaults (CYCLE,
    # OPASSIGN, CHAINSTORE, INLINE), so the fixpoint gate was blind to them.
    opt = "-O2"
    rest = []
    for a in sys.argv[2:]:
        if a.startswith("--opt="):
            opt = a.split("=", 1)[1]
        else:
            rest.append(a)
    for kv in rest:
        k, _, v = kv.partition("=")
        env[k] = v
    # A self-hosted mcc that faults (0xC0000005) must FAIL FAST with its crash
    # code, not hang for the ctest timeout: Windows Error Reporting otherwise
    # intercepts the crash and the child never returns. SEM_NOGPFAULTERRORBOX is
    # inherited by children, mirroring the exec runner (instruction 32) and
    # selfhost-smoke.py. Measured on the arm64-PE self-host (run 30731059619):
    # selfhost-smoke failed in 3.65s with SEM set, selfhost-fixpoint (without it)
    # still hit the 300s timeout.
    if os.name == "nt":
        try:
            import ctypes
            ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
        except Exception:
            pass
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    mcc = os.path.join(root, bdir, "mcc")
    # On win32 the stage compiler is mcc.exe; PE-keyed behaviour (below) follows
    # from that suffix, so the POSIX path is untouched.
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
        # Same backslash repair as selfhost-smoke.py: a TinyCC-toolchain ninja
        # build writes backslash -I paths while still POSIX-escaping quotes, so
        # a plain shlex.split eats the path backslashes. Double every backslash
        # NOT escaping a quote so the paths survive the POSIX split.
        cmd = re.sub(r'\\(?!")', r'\\\\', cmd)
    flags = [a for a in shlex.split(cmd)[1:]
             if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]

    # MCC_EMBED_MCCRT bakes the runtime into the compiler as mccrt_blob.c.o, and
    # every stage links that object. Darwin and WIN32 force the option off and
    # ship the sidecar libmccrt.a instead; a stage compiler lives in a temp dir
    # so its auto-mccdir cannot see it, and -B points it back at the build dir.
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
        # The object suffix is .o on POSIX, .obj under the win32 (clang-cl/ninja)
        # toolchain; take whichever the build produced.
        jbase = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c")
        jitblob = next((jbase + ext for ext in (".o", ".obj")
                        if os.path.exists(jbase + ext)), None)
        if not jitblob:
            sys.exit(f"no JIT blob at {jbase}.o[bj] (build the mcc target first)")
        link_objs.append(jitblob)

    src = os.path.join(root, "src/mcc.c")
    inc = os.path.join(root, "runtime/include")
    # win32 layers runtime/win32/include over runtime/include, so a stage
    # compiler in a temp dir needs the win32 prefix as well to resolve its
    # freestanding + PE system headers; the CRT startup/import libs also come
    # from there at link. POSIX resolves both from the sidecar -B alone.
    win32_pre = ["-B", os.path.join(root, "runtime/win32")] if pe else []
    # msvcrt is mcc's output CRT and mcc auto-links kernel32/msvcrt on PE; there
    # is no -lm/-ldl to add (they do not exist), and a bare -ldl would error.
    link_libs = [] if pe else ["-lm", "-ldl"]

    def compile_mcc(cc_bin, obj, extra_inc):
        args = [cc_bin, *flags]
        if extra_inc:
            args += ["-I" + inc, *win32_pre]
        args += [opt, "-c", src, "-o", obj]
        subprocess.run(args, cwd=root, env=env, check=True)

    def link_mcc(cc_bin, obj, out):
        subprocess.run([cc_bin, *link_flags, *win32_pre, obj, *link_objs,
                        "-o", out, *link_libs],
                       cwd=root, check=True)

    with tempfile.TemporaryDirectory() as work:
        knobs = rest or "(none)"
        o1 = os.path.join(work, "o1.o")
        o2 = os.path.join(work, "o2.o")
        o3 = os.path.join(work, "o3.o")
        exe = ".exe" if pe else ""
        mcc1 = os.path.join(work, "mcc1" + exe)
        mcc2 = os.path.join(work, "mcc2" + exe)

        print(f"fixpoint: stage1 (stage0 mcc compiles+links mcc1)  knobs={knobs}")
        compile_mcc(mcc, o1, False)
        link_mcc(mcc, o1, mcc1)

        print("fixpoint: stage2 (mcc1 compiles+links mcc2)")
        compile_mcc(mcc1, o2, True)
        link_mcc(mcc1, o2, mcc2)

        print("fixpoint: stage3 (mcc2 compiles mcc.c)")
        compile_mcc(mcc2, o3, True)

        b1, b2, b3 = (open(p, "rb").read() for p in (o1, o2, o3))
        print(f"fixpoint: sizes o1={len(b1)} o2={len(b2)} o3={len(b3)}")
        if b2 != b3:
            sys.exit("FAIL: self-host NOT a fixpoint (o2 != o3): unstable self-host codegen")
        if b1 != b2:
            sys.exit("FAIL: stage0 (host-cc-built) and stage1 (mcc-built) mcc emit "
                     "different code (o1 != o2): nondeterministic codegen")

    print("fixpoint: OK (o1 == o2 == o3, byte-identical self-host)")

if __name__ == "__main__":
    main()
