#!/usr/bin/env python3
"""`MCC_MAX_PARSE_DEPTH` is a depth budget priced in host stack bytes; this prices it again.

The guard admits 512 levels of parser recursion.  512 is safe only because a
level was measured to cost at most 992 bytes of host stack in the `-O0` build,
so the whole budget fits in 516 KiB.  Nothing in that sentence is checked by a
compiler: add one array to `unary_nested` and the same 512 levels need a stack
the compiler no longer gets, and every symptom of that is a SIGSEGV in somebody
else's build with no diagnostic attached.

This reads the frame size of every function on the parser's recursion cycles
straight out of the built `mcc`, from the x86-64 prologue -- 8 bytes of return
address, 8 per `push`, plus the `sub $N,%rsp`.  Summing the frames around each
cycle in AXES reproduces the per-level cost measured independently by lowering
the stack rlimit and bisecting the depth at which each axis dies: all thirteen
axes agree to within 0.6%, so the static sum is the same number as the bisect
and is exact, instant, and names the function that moved.

The bisect is kept as `--bisect`, for re-deriving the bank, not as the gate.
Near the threshold it is not deterministic: `sizeofchain` at depth 20,000 fails
15/15 runs at 506 KiB, 7/15 at 512 KiB, 5/15 at 514 KiB and 0/15 at 516 KiB,
because argv, environ and the kernel's stack randomisation all land inside the
rlimit.  A gate whose verdict flips run to run across an 8 KiB band is worse
than no gate; a gate that says `unary_nested grew 832 -> 1344` is better than
both.

AXES is the hand-derived half and the only part not regenerated.  Each entry is
the exact sequence of frames the `-O0` build pushes between two consecutive
`mcc_parse_depth_enter` calls, read off a gdb backtrace taken at
`mcc_parse_depth == 200` on a generated input; `levels` is how many of the 512
one source level of that axis spends.

The bank describes the `-O0` build, which is the worst case and therefore the
one worth banking: the same source built `-O2` needs 452 KiB rather than 516,
because gcc inlines the eight thin `mcc_parse_depth_enter` wrappers into their
`*_nested` bodies.  That also means AXES does not describe an optimised build
at all, which is why a banked function that has stopped existing in the binary
is a failure and not a skip.

Exit 0 clean, 1 on any violation, 2 on usage or a binary this cannot read.
Never 77.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BANK = os.path.join(ROOT, "tests", "diagnostics", "parse-frames.json")

AXES = {
    "blocks":      (1, ["block", "block_nested"]),
    "cast":        (1, ["unary", "unary_nested"]),
    "declfnptr":   (1, ["type_decl_1", "type_decl_nested"]),
    "declparen":   (1, ["type_decl_1", "type_decl_nested"]),
    "derefchain":  (2, ["unary", "unary_nested", "unary", "unary_nested"]),
    "forchain":    (1, ["block", "lblock", "block_nested"]),
    "ifchain":     (1, ["block", "block_nested"]),
    "initbraces":  (1, ["decl_initializer_1", "decl_initializer",
                        "decl_initializer_nested"]),
    "macronest":   (1, ["macro_subst", "macro_arg_subst2", "macro_arg_subst",
                        "macro_subst_tok", "macro_subst_nested"]),
    "paren":       (2, ["unary", "unary_nested", "gexpr", "expr_eq",
                        "expr_cond", "expr_cond_nested"]),
    "sizeofchain": (1, ["unary", "expr_type", "expr_type_vm", "unary_nested"]),
    "structnest":  (1, ["struct_decl", "parse_btype", "struct_decl_nested"]),
    "ternary":     (1, ["expr_cond", "expr_cond_nested"]),
}

BASE_BYTES = 20480
CEILING_BYTES = 768 * 1024
PROVED_AT_KIB = 1024

FN = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")
INSN = re.compile(r"^\s+[0-9a-f]+:\s+(.*)$")
SUB = re.compile(r"^sub \$(0x[0-9a-f]+|[0-9]+),%rsp$")
RSP = re.compile(r"^(sub|and|add) .*,%rsp$")
SKIP = ("endbr64", "mov %rsp,%rbp", "nop", "xchg %ax,%ax", "data16", "cs")


def read_max_depth():
    path = os.path.join(ROOT, "src", "mcc.h")
    m = re.search(r"^#define\s+MCC_MAX_PARSE_DEPTH\s+(\d+)\s*$",
                  open(path, encoding="utf-8", errors="replace").read(), re.M)
    if not m:
        sys.stderr.write("parse-frames: no `#define MCC_MAX_PARSE_DEPTH` in %s. "
                         "The budget this cell prices has been renamed or "
                         "removed, so the bank describes nothing\n" % path)
        sys.exit(2)
    return int(m.group(1))


def enter_sites():
    out = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "src", "*.c"))):
        txt = open(path, encoding="utf-8", errors="replace").read()
        n = len(re.findall(r"\bmcc_parse_depth_enter\s*\(\s*\)\s*;", txt))
        if n:
            out[os.path.basename(path)] = n
    return out


def frames(binary, wanted):
    out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", binary],
                         capture_output=True, text=True)
    if out.returncode != 0:
        sys.stderr.write("parse-frames: objdump -d %s failed:\n%s\n"
                         % (binary, out.stderr[:2000]))
        sys.exit(2)
    got, cur, acc, done = {}, None, 0, True
    for line in out.stdout.splitlines():
        m = FN.match(line)
        if m:
            if cur in wanted:
                got[cur] = acc
            cur, acc, done = m.group(1), 8, False
            continue
        if done or cur is None:
            continue
        m = INSN.match(line)
        if not m:
            continue
        t = " ".join(m.group(1).split("#")[0].split())
        if t.startswith(SKIP):
            continue
        if t.startswith("push %r"):
            acc += 8
            continue
        m = SUB.match(t)
        if m:
            v = m.group(1)
            acc += int(v, 16) if v.startswith("0x") else int(v)
            done = True
            continue
        if RSP.match(t):
            if cur in wanted:
                got[cur] = None
            done = True
            continue
        done = True
    if cur in wanted:
        got[cur] = acc
    return got


def cc_id(build):
    for path in glob.glob(os.path.join(build, "CMakeFiles", "*",
                                       "CMakeCCompiler.cmake")):
        txt = open(path, encoding="utf-8", errors="replace").read()
        cid = re.search(r'set\(CMAKE_C_COMPILER_ID "([^"]*)"\)', txt)
        ver = re.search(r'set\(CMAKE_C_COMPILER_VERSION "([^"]*)"\)', txt)
        if cid:
            return "%s %s" % (cid.group(1), ver.group(1) if ver else "?")
    return "unknown"


def price(got):
    per = {}
    for axis, (levels, cycle) in AXES.items():
        per[axis] = sum(got[fn] for fn in cycle) // levels
    return per


def gen(axis, n):
    return {
        "paren": lambda: "int main(void){return " + "(" * n + "0" + ")" * n + ";}\n",
        "cast": lambda: "int main(void){return " + "(int)" * n + "0;}\n",
        "sizeofchain": lambda: "int main(void){return (int)" + "sizeof " * n + "(0);}\n",
        "derefchain": lambda: "int main(void){int x=0;return " + "*&" * n + "x;}\n",
        "ternary": lambda: "int main(void){return " + "1?0:" * n + "0;}\n",
        "blocks": lambda: "int main(void){" + "{" * n + "return 0;" + "}" * n + "}\n",
        "ifchain": lambda: "int main(void){" + "if(1)" * n + "return 0;return 1;}\n",
        "forchain": lambda: "int main(void){" + "for(;;)" * n + "break;return 0;}\n",
        "initbraces": lambda: ("int a[1] = " + "{" * n + "0" + "}" * n +
                               ";\nint main(void){return a[0];}\n"),
        "declparen": lambda: ("int " + "(" * n + "x" + ")" * n +
                              ";\nint main(void){return 0;}\n"),
        "declfnptr": lambda: ("int " + "(*" * n + "f" + ")(void)" * n +
                              ";\nint main(void){return 0;}\n"),
        "structnest": lambda: ("".join("struct s%d { " % i for i in range(n)) +
                               "int x;" + " };" * n + "\nint main(void){return 0;}\n"),
        "macronest": lambda: ("#define M(x) x\nint main(void){return " + "M(" * n +
                              "0" + ")" * n + ";}\n"),
    }[axis]()


def bisect(mcc, build, work, depth):
    os.makedirs(work, exist_ok=True)
    res = {}
    for axis in sorted(AXES):
        src = os.path.join(work, axis + ".c")
        open(src, "w").write(gen(axis, depth))
        cmd = "ulimit -s %%d; exec %s -B%s -c %s -o %s.o" % (mcc, build, src, src)
        lo, hi = 32, 4096
        while lo < hi:
            mid = (lo + hi) // 2
            p = subprocess.run(["sh", "-c", cmd % mid], capture_output=True, text=True)
            ok = (0 < p.returncode < 128 and
                  "program nests too deeply" in (p.stdout + p.stderr))
            if ok:
                hi = mid
            else:
                lo = mid + 1
        res[axis] = lo
        print("parse-frames: bisect %-12s %4d KiB" % (axis, lo))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("--mcc")
    ap.add_argument("--bank", default=BANK)
    ap.add_argument("--update-bank", action="store_true")
    ap.add_argument("--bisect", action="store_true")
    ap.add_argument("--bisect-depth", type=int, default=20000)
    ap.add_argument("--tol-bytes-per-level", type=int, default=16)
    a = ap.parse_args()

    try:
        bank = json.load(open(a.bank, encoding="utf-8"))
    except (OSError, ValueError) as exc:
        sys.stderr.write("parse-frames: cannot read the bank %s: %s\n" % (a.bank, exc))
        return 2

    mcc = a.mcc or os.path.join(a.build, "mcc")
    if not os.path.exists(mcc):
        sys.stderr.write("parse-frames: no mcc at %s\n" % mcc)
        return 2

    depth_now = read_max_depth()
    names = sorted({fn for _, cycle in AXES.values() for fn in cycle})
    got = frames(mcc, set(names))
    sites = enter_sites()

    bad = []
    missing = [n for n in names if n not in got]
    if missing:
        bad.append("%d function(s) on the banked recursion cycles are not in %s "
                   "at all: %s. Either they were renamed, or this mcc was built "
                   "with optimisation and the wrappers were inlined away -- "
                   "either way tools/parse-frames.py is now pricing a binary "
                   "that does not exist and has stopped watching anything"
                   % (len(missing), mcc, ", ".join(missing)))
    dynamic = sorted(n for n, v in got.items() if v is None)
    if dynamic:
        bad.append("%s adjust %%rsp by a register (alloca, or a VLA), so their "
                   "frames are not a constant and the per-level cost below is "
                   "not a number. A variable-size frame on a cycle bounded only "
                   "by a fixed depth count cannot be sized at all"
                   % ", ".join(dynamic))
    if bad:
        for b in bad:
            print("parse-frames: " + b)
        return 1

    per = price(got)
    worst = max(sorted(per), key=lambda k: per[k])
    unit = per[worst]
    need = depth_now * unit + BASE_BYTES

    if a.update_bank:
        out = {"compiler": cc_id(a.build), "enter_sites": sites,
               "frames": {n: got[n] for n in names},
               "max_parse_depth": depth_now, "need_bytes": need,
               "per_level_bytes": per, "worst_axis": worst}
        if a.bisect:
            out["bisect_kib"] = bisect(mcc, a.build,
                                       os.path.join(a.build, "parse-frames-work"),
                                       a.bisect_depth)
        elif "bisect_kib" in bank:
            out["bisect_kib"] = bank["bisect_kib"]
        with open(a.bank, "w", encoding="utf-8") as fh:
            json.dump(out, fh, indent=1, sort_keys=True)
            fh.write("\n")
        print("parse-frames: banked %d frames on %d cycles, worst axis %s at %d "
              "B/level, %d levels need %d KiB"
              % (len(names), len(AXES), worst, unit, depth_now, need // 1024))
        return 0

    if a.bisect:
        bisect(mcc, a.build, os.path.join(a.build, "parse-frames-work"),
               a.bisect_depth)

    if sites != bank["enter_sites"]:
        print("parse-frames: mcc_parse_depth_enter() is called from %s, banked "
              "%s. A recursion point was guarded, moved or dropped since the "
              "frames were priced, and AXES in tools/parse-frames.py still "
              "prices %d cycles over the old set -- so the new one's per-level "
              "cost is in nobody's budget. Add its cycle to AXES (a gdb "
              "backtrace at `mcc_parse_depth == 200` gives it) and re-derive"
              % (", ".join("%s x%d" % kv for kv in sorted(sites.items())),
                 ", ".join("%s x%d" % kv for kv in sorted(bank["enter_sites"].items())),
                 len(AXES)))
        return 1

    if depth_now != bank["max_parse_depth"]:
        print("parse-frames: MCC_MAX_PARSE_DEPTH is %d in src/mcc.h and %d in %s. "
              "The budget moved without the stack it costs being re-derived: %d "
              "levels of the %s axis need %d KiB, and tests/diagnostics/"
              "parse-depth.sh only proves the diagnostic fires at a %d KiB rlimit"
              % (depth_now, bank["max_parse_depth"], os.path.basename(a.bank),
                 depth_now, worst, need // 1024, PROVED_AT_KIB))
        return 1

    tol = depth_now * a.tol_bytes_per_level
    banked_need = bank["need_bytes"]
    moved = sorted(((n, bank["frames"].get(n, 0), got[n]) for n in names
                    if got[n] != bank["frames"].get(n)),
                   key=lambda t: t[2] - t[1], reverse=True)

    over_drift = need > banked_need + tol
    over_ceiling = need > CEILING_BYTES
    here = cc_id(a.build)
    if here != bank["compiler"]:
        print("parse-frames: the bank was taken from %r and this build dir is "
              "%r; frame drift is not comparable across compilers, so only the "
              "%d KiB ceiling is checked below"
              % (bank["compiler"], here, CEILING_BYTES // 1024))
        over_drift = False

    if not over_drift and not over_ceiling:
        print("parse-frames: OK -- %d frames on %d recursion cycles, worst axis "
              "%s at %d B per mcc_parse_depth level; MCC_MAX_PARSE_DEPTH %d needs "
              "%d KiB of host stack, banked %d KiB, ceiling %d KiB"
              % (len(names), len(AXES), worst, unit, depth_now, need // 1024,
                 banked_need // 1024, CEILING_BYTES // 1024))
        if moved:
            print("parse-frames: inside the %d B/level tolerance, but these "
                  "frames moved: %s"
                  % (a.tol_bytes_per_level,
                     ", ".join("%s %d->%d" % (n, o, w) for n, o, w in moved)))
        return 0

    for n, old, new in moved[:6]:
        on = sorted(ax for ax, (_, cyc) in AXES.items() if n in cyc)
        print("parse-frames: %s grew %d -> %d B (%+d) and is on %d of %d "
              "recursion cycles (%s)"
              % (n, old, new, new - old, len(on), len(AXES), ", ".join(on)))
    print("parse-frames: the worst axis is %s at %d B per mcc_parse_depth level, "
          "banked %d for %s"
          % (worst, unit, bank["per_level_bytes"][bank["worst_axis"]],
             bank["worst_axis"]))
    print("parse-frames: MCC_MAX_PARSE_DEPTH %d therefore needs %d KiB of host "
          "stack, banked %d KiB, tolerance %d B/level (%d KiB)"
          % (depth_now, need // 1024, banked_need // 1024,
             a.tol_bytes_per_level, tol // 1024))
    if over_ceiling:
        print("parse-frames: that is past the %d KiB ceiling. tests/diagnostics/"
              "parse-depth.sh runs mcc at a %d KiB rlimit and is the cell that "
              "proves the diagnostic fires instead of the guard page; once the "
              "requirement passes that, it fails as thirteen SIGSEGVs at once "
              "and names no cause" % (CEILING_BYTES // 1024, PROVED_AT_KIB))
    print("parse-frames: a reader who did not run this would conclude that "
          "MCC_MAX_PARSE_DEPTH %d still has the %.2fx margin over a %d KiB stack "
          "it was sized with. It has %.2fx"
          % (depth_now, PROVED_AT_KIB * 1024.0 / banked_need, PROVED_AT_KIB,
             PROVED_AT_KIB * 1024.0 / need))
    fits = max(1, (banked_need - BASE_BYTES) // unit)
    print("parse-frames: shrink the frame, or lower MCC_MAX_PARSE_DEPTH to %d, "
          "which is what still fits in the %d KiB this was banked at (%s -- C11 "
          "5.2.4.1 costs 256 levels with every nesting minimum in force at once, "
          "and tests/diagnostics/parse-depth.sh compiles that program), or "
          "accept the cost and re-derive with `tools/parse-frames.py %s --bisect "
          "--update-bank`"
          % (fits, banked_need // 1024,
             "still conforming" if fits >= 256 else "BELOW CONFORMANCE",
             a.build))
    return 1


if __name__ == "__main__":
    sys.exit(main())
