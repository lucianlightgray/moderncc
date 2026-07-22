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
import json, os, shlex, subprocess, sys, tempfile

def main():
    if len(sys.argv) < 2:
        sys.exit("usage: selfhost-fixpoint.py <build-dir> [KNOB=VAL ...]")
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

    link_objs = [blob]
    if any(a.startswith("-DMCC_EMBED_JIT_BLOB") for a in flags):
        jitblob = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c.o")
        if not os.path.exists(jitblob):
            sys.exit(f"no JIT blob at {jitblob} (build the mcc target first)")
        link_objs.append(jitblob)

    src = os.path.join(root, "src/mcc.c")
    inc = os.path.join(root, "runtime/include")

    def compile_mcc(cc_bin, obj, extra_inc):
        args = [cc_bin, *flags]
        if extra_inc:
            args.append("-I" + inc)
        args += ["-O2", "-c", src, "-o", obj]
        subprocess.run(args, cwd=root, env=env, check=True)

    def link_mcc(cc_bin, obj, out):
        subprocess.run([cc_bin, obj, *link_objs, "-o", out, "-lm", "-ldl"],
                       cwd=root, check=True)

    with tempfile.TemporaryDirectory() as work:
        knobs = sys.argv[2:] or "(none)"
        o1 = os.path.join(work, "o1.o")
        o2 = os.path.join(work, "o2.o")
        o3 = os.path.join(work, "o3.o")
        mcc1 = os.path.join(work, "mcc1")
        mcc2 = os.path.join(work, "mcc2")

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
