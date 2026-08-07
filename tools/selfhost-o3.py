#!/usr/bin/env python3
"""Self-host mcc at -O3: compile src/mcc.c with a built mcc at -O3, link with mcc
itself (its runtime supplies the x87 long-double helpers GNU ld cannot resolve),
and verify the resulting compiler builds correct executables.

Usage: tools/selfhost-o3.py <build-dir> <out-binary> [-O3|-O2|...]
"""
import json, os, shlex, subprocess, sys

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
    if len(sys.argv) < 3:
        sys.exit("usage: selfhost-o3.py <build-dir> <out-binary> [-Olevel]")
    bdir, out = sys.argv[1], os.path.abspath(sys.argv[2])
    olevel = sys.argv[3] if len(sys.argv) > 3 else "-O3"
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    mcc = os.path.join(root, bdir, "mcc")
    if not os.access(mcc, os.X_OK):
        sys.exit(f"no mcc at {mcc}")

    cc = json.load(open(os.path.join(root, bdir, "compile_commands.json")))
    rec = [x for x in cc if x["file"].endswith("/mcc.c")][0]
    flags = [a for a in shlex.split(rec["command"])[1:]
             if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]

    blob = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccrt_blob.c.o")
    link_objs = [blob] if os.path.exists(blob) else []
    if any(a.startswith("-DMCC_EMBED_JIT_BLOB") for a in flags):
        j = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c.o")
        if not os.path.exists(j):
            sys.exit(f"no JIT blob at {j}")
        link_objs.append(j)

    obj = out + ".o"
    print(f"self-host: {mcc} {olevel} -c src/mcc.c", flush=True)
    subprocess.run([mcc, *flags, olevel, "-c", os.path.join(root, "src/mcc.c"),
                    "-o", obj], cwd=root, check=True)
    print(f"self-host: linking -> {out}", flush=True)
    link_libs = selfhost_link_libs(os.path.join(root, bdir))
    subprocess.run([mcc, obj, *link_objs, "-o", out, *link_libs],
                   cwd=root, check=True)

    inc = os.path.join(root, "runtime/include")
    work = os.path.dirname(out)
    tc, te = os.path.join(work, "sh_t.c"), os.path.join(work, "sh_t")
    open(tc, "w").write("#include <stdio.h>\n"
                        "int fib(int n){return n<2?n:fib(n-1)+fib(n-2);}\n"
                        "int main(void){printf(\"%d\\n\",fib(10));return 0;}\n")
    subprocess.run([out, "-I" + inc, tc, "-o", te], check=True)
    got = subprocess.run([te], capture_output=True, text=True).stdout.strip()
    if got != "55":
        sys.exit(f"FAIL: self-hosted mcc gave fib(10)={got}, expected 55")

    qs = os.path.join(root, "tests/exec/programs/quicksort.c")
    qe = os.path.join(work, "sh_qs")
    subprocess.run([out, "-I" + inc, qs, "-o", qe], check=True)
    sortd = subprocess.run([qe], capture_output=True, text=True).stdout.strip().splitlines()[-1]
    if "4 16 21 33 36 37 38 53 55 62 65 74 74 83 89 96" not in sortd:
        sys.exit(f"FAIL: self-hosted mcc mis-sorted quicksort: {sortd}")

    print(f"self-host: OK ({out} at {olevel})")

if __name__ == "__main__":
    main()
