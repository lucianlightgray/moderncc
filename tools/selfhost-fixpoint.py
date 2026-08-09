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

Usage: tools/selfhost-fixpoint.py <build-dir> [--opt=-ON] [-fflag ...]

The trailing arguments are compiler FLAGS and go on argv. They used to be
`KNOB=VAL` environment knobs; the optimizer knobs moved to argv (MCC_OPT_ROW in
src/mccopt.h) and the four ctest cells that drive this file were updated to pass
`-f` spellings, but this file was not -- so `-fdivmagic` became an environment
variable named `-fdivmagic` that nothing reads, and selfhost-fixpoint-gates and
the three selfhost-fixpoint-memmodel-* cells were byte-for-byte reruns of the
plain cell while printing `knobs=[...]` as if they were not. Non-flag arguments
are refused rather than silently ignored.
"""
import json, os, re, shlex, subprocess, sys, tempfile

def selfhost_link_libs(bdir, pe=False):
    """The libraries a self-hosted mcc must link, as CMake resolved them.

    A self-hosted mcc is the same program as the mcc target, so it needs the same
    libraries; hardcoding a list here means the two drift the moment an option
    adds one. MCC_GPU used to be exactly that case: it put a hard Vulkan
    dependency in the compiler's link, and every self-host recipe failed on the
    vk* symbols until CMake told it. The backend is dlopened now, so MCC_GPU
    contributes nothing here; the file stays the single source of truth.

    CMake writes <build-dir>/selfhost-link-libs.txt from the same variables it
    passes to target_link_libraries, under the same MCC_TARGETOS condition, so
    there is one source of truth. The fallback is what this recipe hardcoded
    before the file existed, for a build tree configured by an older CMake.
    """
    p = os.path.join(bdir, "selfhost-link-libs.txt")
    if not os.path.exists(p):
        return [] if pe else ["-lm", "-ldl"]
    with open(p) as f:
        return [ln.strip() for ln in f if ln.strip()]

def main():
    if len(sys.argv) < 2:
        sys.exit("usage: selfhost-fixpoint.py <build-dir> [--opt=-ON] [-fflag ...]")
    bdir = sys.argv[1]
    env = dict(os.environ)
    opt = "-O2"
    rest = []
    for a in sys.argv[2:]:
        if a.startswith("--opt="):
            opt = a.split("=", 1)[1]
        else:
            rest.append(a)
    for a in rest:
        if not a.startswith("-"):
            sys.exit("selfhost-fixpoint: %r is not a compiler flag. The "
                     "retired KNOB=VAL spelling reached the environment and no "
                     "part of mcc, so the run would be a byte-for-byte rerun of "
                     "the default configuration while printing knobs=[...]" % a)
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

    src = os.path.join(root, "src/mcc.c")
    inc = os.path.join(root, "runtime/include")
    win32_pre = ["-B", os.path.join(root, "runtime/win32")] if pe else []
    link_libs = selfhost_link_libs(os.path.join(root, bdir), pe)

    def compile_mcc(cc_bin, obj, extra_inc):
        args = [cc_bin, *flags]
        if extra_inc:
            args += ["-I" + inc, *win32_pre]
        args += [opt, *rest, "-c", src, "-o", obj]
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
