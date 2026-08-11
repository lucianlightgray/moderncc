#!/usr/bin/env python3
"""A `-g --embed-jit` build bakes, hot-swaps, and stays debuggable under gdb.

Two things are pinned here, and the second is a limit rather than a feature.

WHAT WORKS.  `-g` used to suppress JIT baking outright: rir_hook_body_begin
cleared rir_try_active whenever debug_modes was set, so a debug build carried no
runtime JIT engine and said so only in a warning ("baking needs -O1+ and is
disabled by -g/-ftest-coverage").  A debug build is exactly where a hot-swapped
body most needs to stay inspectable, so --embed-jit now overrides debug_modes and
is the only thing gating the bake.  This test asserts a -g stage2 boots and swaps
(it was boot=0 before), and that the resulting binary is genuinely debuggable:
breakpoints resolve to file:line, arguments are typed and readable, and a
backtrace crosses from the generated boot function into the JIT engine with
source locations at every frame.

WHAT DOES NOT WORK, AND IS ASSERTED SO IT CANNOT BE FORGOTTEN.  The hot-patched
code itself carries no symbols.  After boot a swapped slot points into an
anonymous mapping and `info symbol` answers "No symbol matches"; mcc implements
no GDB JIT interface (there is no __jit_debug_register_code anywhere in src/), so
gdb cannot name a JIT variant, show its source, or step it.  Debuggability is
preserved for everything the AOT image covers and stops exactly at the swap.

That boundary is the point.  If someone adds __jit_debug_register_code, the last
assertion here fails, and it should -- the answer to "is hot-patched code
debuggable?" will have changed and this file must be rewritten rather than
silenced.

SKIPs (77) without gdb, without a baked JIT engine, or without a runtime blob.

Usage: tools/jit-gdb-debuggable.py <build-dir>
"""
import json, os, re, shlex, shutil, subprocess, sys, tempfile

SKIP = 77


def die(msg):
    print("jit-gdb: " + msg)
    sys.exit(1)


def skip(msg):
    print("SKIP: jit-gdb: " + msg)
    sys.exit(SKIP)


def flags_for_mcc_c(bdir):
    cc = json.load(open(os.path.join(bdir, "compile_commands.json")))
    rec = [x for x in cc if x["file"].endswith("/mcc.c")]
    if not rec:
        die("no src/mcc.c entry in compile_commands.json")
    return [a for a in shlex.split(rec[0]["command"])[1:]
            if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]


def gdb_batch(gdb, exe, script, args):
    with tempfile.NamedTemporaryFile("w", suffix=".gdb", delete=False) as f:
        f.write(script)
        path = f.name
    try:
        p = subprocess.run([gdb, "-batch", "-nx", "-x", path, "--args", exe, *args],
                           capture_output=True, text=True, timeout=600)
        return p.stdout + p.stderr
    finally:
        os.unlink(path)


def main():
    if len(sys.argv) < 2:
        die("usage: tools/jit-gdb-debuggable.py <build-dir>")
    bdir = os.path.abspath(sys.argv[1])
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    gdb = shutil.which("gdb")
    if not gdb:
        skip("no gdb on PATH")
    mcc = os.path.join(bdir, "mcc")
    if not os.access(mcc, os.X_OK):
        skip("no mcc in %s" % bdir)
    jitblob = os.path.join(bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c.o")
    rtsrc = os.path.join(bdir, "mccrt_blob.c")
    if not os.path.exists(jitblob):
        skip("no baked JIT engine (mccjit_blob.c.o)")
    if not os.path.exists(rtsrc):
        skip("no runtime blob source")

    flags = flags_for_mcc_c(bdir)
    fails = []

    with tempfile.TemporaryDirectory() as work:
        rtobj = os.path.join(work, "rt.o")
        if subprocess.run([mcc, "-c", rtsrc, "-o", rtobj], cwd=root,
                          capture_output=True).returncode != 0:
            die("could not compile the runtime blob")

        # Compile AND link in one call: mccjit_embed_finalize runs at link time,
        # so a split build yields a binary with no engine and no diagnostic.
        exe = os.path.join(work, "mcc-g")
        p = subprocess.run([mcc, *flags, "-O0", "-g", "--embed-jit", "-B", bdir,
                            os.path.join(root, "src/mcc.c"), rtobj, jitblob,
                            "-o", exe, "-lm", "-ldl"],
                           cwd=root, capture_output=True, text=True)
        if p.returncode != 0:
            die("the -O0 -g --embed-jit build failed:\n%s" % p.stderr[-2000:])
        if "no functions were JIT-baked" in p.stderr:
            fails.append(
                "-g --embed-jit baked nothing and mcc said so. debug_modes is "
                "suppressing the bake again; --embed-jit is supposed to be the "
                "only gate")

        wl = os.path.join(work, "wl.c")
        open(wl, "w").write("int main(void){return 0;}\n")
        obj = os.path.join(work, "wl.o")
        env = dict(os.environ, MCC_JIT_VERBOSE="1", MCC_JIT="1",
                   MCC_JIT_HOT_CALLS="1")
        r = subprocess.run([exe, "-c", wl, "-o", obj], capture_output=True,
                           text=True, env=env)
        if r.returncode != 0:
            die("the -g stage2 failed to compile a workload:\n%s" % r.stderr[-2000:])
        boot = r.stderr.count("mccjit-boot")
        swap = r.stderr.count("swapped")
        print("jit-gdb: -O0 -g --embed-jit stage2: boot=%d swapped=%d" % (boot, swap))
        if boot <= 0 or swap <= 0:
            fails.append(
                "a -g --embed-jit stage2 booted %d sites and swapped %d. Before "
                "the gate was relaxed this was 0/0; if it is 0/0 again, -g is "
                "suppressing the bake" % (boot, swap))

        # A slot that actually received a variant: entry is a real address.
        slot = None
        for ln in r.stderr.splitlines():
            if "mccjit-boot" in ln and "entry=(nil)" not in ln:
                m = re.search(r"slot=(0x[0-9a-f]+)", ln)
                if m:
                    slot = m.group(1)
                    break
        if slot is None:
            fails.append("no boot line installed a variant, so there is no "
                         "hot-patched slot to inspect")

        # ---- debuggability of the AOT image, with the JIT live ---------------
        script = ("set confirm off\nset pagination off\n"
                  "set environment MCC_JIT=1\nset environment MCC_JIT_HOT_CALLS=1\n"
                  "break mccjit_boot_swap_run\n"
                  "run\ninfo args\nbt 3\n")
        out = gdb_batch(gdb, exe, script, ["-c", wl, "-o", obj])
        if "mccjit_boot_swap_run" not in out or "src/mccjit_embed.c" not in out:
            fails.append(
                "gdb could not stop in mccjit_boot_swap_run with a source "
                "location. A -g build whose JIT engine has no line info is not "
                "the debuggable build this change was made for:\n%s"
                % out[-1200:])
        if not re.search(r"#1 .*mccjit_boot_swap .*src/mccjit_embed\.c", out):
            fails.append(
                "the backtrace did not cross from the boot swap into the engine "
                "with a source location:\n%s" % out[-1200:])
        if "len =" not in out:
            fails.append("gdb printed no typed arguments at the breakpoint; "
                         "`info args` is what makes a stop useful:\n%s"
                         % out[-800:])

        script = ("set confirm off\nset pagination off\n"
                  "set environment MCC_JIT=1\nset environment MCC_JIT_HOT_CALLS=1\n"
                  "break main\nrun\n")
        if slot:
            script += ('printf "SLOT=%p\\n", *(void**)' + slot + "\n"
                       "info symbol *(void**)" + slot + "\n")
        out2 = gdb_batch(gdb, exe, script, ["-c", wl, "-o", obj])
        if not re.search(r"main .*at .*src/mcc\.c:\d+", out2):
            fails.append("gdb could not stop at main with a file:line "
                         "location:\n%s" % out2[-800:])

        # ---- the limit: hot-patched code is not symbolized ------------------
        if slot:
            m = re.search(r"SLOT=(0x[0-9a-f]+)", out2)
            print("jit-gdb: swapped slot %s -> %s" % (slot, m.group(1) if m else "?"))
            probed = "No symbol matches" in out2 or " in section " in out2
            if not probed:
                fails.append(
                    "the `info symbol` probe never ran, so this test learned "
                    "NOTHING about whether hot-patched code is symbolized. Do "
                    "not read a missing answer as a negative one:\n%s"
                    % out2[-800:])
            elif "No symbol matches" not in out2:
                fails.append(
                    "gdb resolved a symbol for the hot-patched slot. That would "
                    "mean JIT variants are now registered with the debugger -- "
                    "check for __jit_debug_register_code and REWRITE this test, "
                    "because 'hot-patched code is not debuggable' would no "
                    "longer be true:\n%s" % out2[-800:])
            else:
                print("jit-gdb: `info symbol` on the swapped slot: no symbol "
                      "(mcc implements no GDB JIT interface)")

    if fails:
        for f in fails:
            print("FAIL jit-gdb: " + f)
        return 1
    print("jit-gdb: OK -- -g --embed-jit bakes and swaps, the image is debuggable "
          "(file:line, typed args, backtrace through the engine), and the "
          "hot-patched code itself is unsymbolized, as recorded")
    return 0


if __name__ == "__main__":
    sys.exit(main())
