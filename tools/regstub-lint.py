#!/usr/bin/env python3
"""Every branch of a capability gate registers the same set of cells.

A cell that is never registered is strictly worse than one that skips.  A skip
appears in the results file, in `ctest -N`, in `tools/must-run.py` and in every
count this project quotes.  An unregistered cell appears nowhere: the suite is
green with fewer tests in it and nothing says so.  That is not hypothetical --
`if(TARGET mcc-<arch>)` with no `else()` dropped 164 cells (144 `optfire-*` and
20 `*-docker`) and was found by accident, months later.

The rule is one sentence and it is structural, not a list of known offenders.
Read CMakeLists.txt and cmake/*.cmake; recover which variables are
*probe-rooted*, meaning reachable by assignment or by an enclosing gate from
`find_program`/`find_path`/`find_library`/`find_file`, `find_package`,
`execute_process`, `option`, `mcc_config_node`, a `set(... CACHE ...)`,
`if(TARGET ...)`, or an `if(EXISTS ...)` on a path outside the source tree;
then require that every `if()/elseif()/else()` chain registering a test and
consulting such a variable register **the same set of cell names on every
branch**, counting the implicit absent `else()` as the empty set.

Names are compared as written, with `${...}` left in place, so a branch that
registers `optfire-${_cpu}/${_cell}` is matched only by a branch that registers
`optfire-${_cpu}/${_cell}`.  That is what catches a stub filed under a name
unrelated to the cells it stands in for -- seven `macho-libsystem/*` cells
whose off-branch was a single `macho-libsystem-kernel-fused`, so no
`tests/must-run.txt` row could ever name both states -- and it is why the check
does not need to know what any particular gate is for.

Two exemptions, both stated rather than enumerated:

  * The chain containing `enable_testing()`.  With testing off there is no
    suite to count.
  * Variables that name *the target that was asked for* rather than the
    equipment of the machine doing the asking (IDENTITY below).  A build for
    WIN32/arm64 and a build for Linux/x86_64 are two different suites and are
    not required to register the same cells; a Linux/x86_64 build with docker
    and one without are the same suite, and must.

Exit 0 clean, 1 on any violation, 2 on a usage or parse problem.  Never 77: the
cell exists to make an invisible registration visible, so it must not skip.
"""

import argparse
import os
import re
import sys

FIND_CMDS = ("find_program", "find_path", "find_library", "find_file")
REG_CMDS = ("add_test", "mcc_skip_test")
IDENT = re.compile(r"^[A-Za-z_][A-Za-z_0-9]*$")
VARREF = re.compile(r"\$\{([A-Za-z_][A-Za-z_0-9]*)\}")

IDENTITY = {
    "WIN32", "UNIX", "APPLE", "MSVC", "MINGW", "CYGWIN", "BORLAND", "IOS",
    "ANDROID", "CMAKE_SYSTEM_NAME", "CMAKE_SYSTEM_PROCESSOR",
    "CMAKE_SYSTEM_VERSION", "CMAKE_SIZEOF_VOID_P", "CMAKE_CROSSCOMPILING",
    "CMAKE_CROSSCOMPILING_EMULATOR", "CMAKE_HOST_UNIX", "CMAKE_HOST_WIN32",
    "CMAKE_HOST_APPLE", "CMAKE_HOST_SYSTEM_NAME", "CMAKE_HOST_SYSTEM_PROCESSOR",
    "CMAKE_C_COMPILER_ID", "CMAKE_C_COMPILER_VERSION", "CMAKE_GENERATOR",
    "CMAKE_BUILD_TYPE", "CMAKE_C_BYTE_ORDER",
    "MCC_TARGET_ARCH", "MCC_TARGET_OS", "MCC_CPU", "MCC_TARGETOS",
    "MCC_TARGET_IS_HOST", "MCC_EMULATOR", "MCC_TRIPLET", "MCC_CC_NAME",
    "MCC_EXESUF", "MCC_LIBSUF", "MCC_DLLSUF", "MCC_CONFIG_MINGW",
    "MCC_ARM_CPUVER", "MCC_GCC_MAJOR",
}

IF_KEYWORDS = {
    "NOT", "AND", "OR", "TARGET", "EXISTS", "DEFINED", "COMMAND", "POLICY",
    "TEST", "IS_DIRECTORY", "IS_SYMLINK", "IS_ABSOLUTE", "MATCHES", "LESS",
    "GREATER", "EQUAL", "LESS_EQUAL", "GREATER_EQUAL", "STRLESS", "STRGREATER",
    "STREQUAL", "STRLESS_EQUAL", "STRGREATER_EQUAL", "VERSION_LESS",
    "VERSION_GREATER", "VERSION_EQUAL", "VERSION_LESS_EQUAL",
    "VERSION_GREATER_EQUAL", "IN_LIST", "ON", "OFF", "TRUE", "FALSE", "YES",
    "NO", "IS_NEWER_THAN", "CACHE", "ENV",
}

INTREE = ("${CMAKE_CURRENT_SOURCE_DIR}", "${CMAKE_SOURCE_DIR}",
          "${PROJECT_SOURCE_DIR}", "${CMAKE_CURRENT_LIST_DIR}")


def parse(txt):
    n, i, line = len(txt), 0, 1
    out = []
    while i < n:
        c = txt[i]
        if c == "\n":
            line += 1
            i += 1
        elif c in " \t\r":
            i += 1
        elif c == "#":
            if txt.startswith("#[[", i):
                j = txt.find("]]", i)
                j = n if j < 0 else j + 2
            else:
                j = txt.find("\n", i)
                j = n if j < 0 else j
            line += txt.count("\n", i, j)
            i = j
        elif c.isalpha() or c == "_":
            j = i
            while j < n and (txt[j].isalnum() or txt[j] == "_"):
                j += 1
            k = j
            while k < n and txt[k] in " \t\r\n":
                k += 1
            if k >= n or txt[k] != "(":
                line += txt.count("\n", i, j)
                i = j
                continue
            name, startline = txt[i:j], line + txt.count("\n", i, k)
            p, depth = k + 1, 1
            while p < n and depth:
                ch = txt[p]
                if ch == '"':
                    p += 1
                    while p < n and txt[p] != '"':
                        p += 2 if txt[p] == "\\" else 1
                    p += 1
                    continue
                if txt.startswith("[[", p):
                    e = txt.find("]]", p)
                    p = n if e < 0 else e + 2
                    continue
                if ch == "#":
                    e = txt.find("\n", p)
                    p = n if e < 0 else e
                    continue
                depth += (ch == "(") - (ch == ")")
                p += 1
            out.append((name, startline, txt[k + 1:p - 1]))
            line += txt.count("\n", i, p)
            i = p
        else:
            i += 1
    return out


def split_args(args):
    out, cur, i, n, inq, started = [], "", 0, len(args), False, False
    while i < n:
        c = args[i]
        if c == '"':
            inq = not inq
            started = True
            i += 1
            continue
        if not inq and c in " \t\r\n":
            if started:
                out.append(cur)
            cur, started = "", False
            i += 1
            continue
        if c == "\\" and i + 1 < n:
            cur += args[i:i + 2]
            started = True
            i += 2
            continue
        cur += c
        started = True
        i += 1
    if started:
        out.append(cur)
    return out


def cond_vars(args):
    seen = set(VARREF.findall(args))
    for t in split_args(args):
        if IDENT.match(t) and t not in IF_KEYWORDS:
            seen.add(t)
    return seen


def gate_reason(args, probe, literals):
    toks = split_args(args)
    for idx, t in enumerate(toks):
        if t == "TARGET":
            return "if(TARGET ...), and whether a target exists is a configure outcome"
        if t == "EXISTS" and idx + 1 < len(toks):
            path = toks[idx + 1]
            m = VARREF.fullmatch(path.strip())
            if m and m.group(1) in literals:
                path = literals[m.group(1)]
            if not any(path.startswith(p) for p in INTREE):
                return "if(EXISTS %s), a path outside the source tree" % path[:60]
    for v in sorted(cond_vars(args)):
        if v in probe:
            return "%s, %s" % (v, probe[v])
    return None


def scan(paths, root):
    files = {}
    for p in paths:
        files[p] = parse(open(p, encoding="utf-8", errors="replace").read())

    probe, literals, loopvars = {}, {}, set()

    def rel(p):
        return os.path.relpath(p, root)

    def seed(var, why):
        if IDENT.match(var) and var not in IDENTITY and var not in probe:
            probe[var] = why

    for p, cmds in files.items():
        for name, ln, args in cmds:
            low, toks = name.lower(), split_args(args)
            if not toks:
                continue
            if low in FIND_CMDS:
                seed(toks[0], "found by %s at %s:%d" % (low, rel(p), ln))
            elif low == "find_package":
                seed(toks[0] + "_FOUND", "set by find_package(%s) at %s:%d"
                     % (toks[0], rel(p), ln))
            elif low in ("option", "mcc_config_node"):
                seed(toks[0], "declared by %s at %s:%d" % (low, rel(p), ln))
            elif low == "execute_process":
                for kw in ("RESULT_VARIABLE", "OUTPUT_VARIABLE", "ERROR_VARIABLE"):
                    if kw in toks and toks.index(kw) + 1 < len(toks):
                        seed(toks[toks.index(kw) + 1],
                             "captured from execute_process at %s:%d" % (rel(p), ln))
            elif low == "set":
                if "CACHE" in toks:
                    seed(toks[0], "a cache entry set at %s:%d" % (rel(p), ln))
                elif len(toks) == 2 and IDENT.match(toks[0]):
                    literals.setdefault(toks[0], toks[1])
            elif low == "foreach":
                loopvars.add(toks[0])

    tainting = {}
    for _ in range(24):
        grew = False
        for p, cmds in files.items():
            stack, fn, params = [], None, []
            for name, ln, args in cmds:
                low = name.lower()
                if low in ("function", "macro"):
                    t = split_args(args)
                    fn = t[0] if t else None
                    params = t[1:]
                elif low in ("endfunction", "endmacro"):
                    fn, params = None, []
                elif low == "if":
                    stack.append(gate_reason(args, probe, literals) is not None)
                elif low == "elseif" and stack:
                    stack[-1] = stack[-1] or gate_reason(args, probe, literals) is not None
                elif low == "endif" and stack:
                    stack.pop()
                elif low == "set":
                    toks = split_args(args)
                    if not toks:
                        continue
                    tgt = toks[0]
                    hot = (stack and stack[-1]) or any(
                        v in probe for v in VARREF.findall(" ".join(toks[1:])))
                    if not hot:
                        continue
                    why = "derived from a probe at %s:%d" % (rel(p), ln)
                    if IDENT.match(tgt):
                        if tgt not in IDENTITY and tgt not in loopvars and tgt not in probe:
                            probe[tgt] = why
                            grew = True
                    elif fn and "PARENT_SCOPE" in toks and VARREF.fullmatch(tgt):
                        pname = VARREF.fullmatch(tgt).group(1)
                        if pname in params:
                            slot = tainting.setdefault(fn, set())
                            if params.index(pname) not in slot:
                                slot.add(params.index(pname))
                                grew = True
                elif low in tainting:
                    call = split_args(args)
                    for pos in tainting[low]:
                        if pos >= len(call):
                            continue
                        t = call[pos]
                        if (IDENT.match(t) and t not in IDENTITY
                                and t not in loopvars and t not in probe):
                            probe[t] = "an out-parameter of %s()" % low
                            grew = True
        if not grew:
            break
    return files, probe, literals


class Chain(object):
    def __init__(self, line, cond):
        self.line, self.cond = line, cond
        self.branches = [{"kind": "if", "line": line, "names": []}]
        self.escaped = False

    def cur(self):
        return self.branches[-1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", nargs="?", default=".")
    ap.add_argument("--min-chains", type=int, default=25)
    a = ap.parse_args()

    root = os.path.abspath(a.source)
    paths = [os.path.join(root, "CMakeLists.txt")]
    d = os.path.join(root, "cmake")
    if os.path.isdir(d):
        paths += [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.endswith(".cmake")]
    missing = [p for p in paths if not os.path.isfile(p)]
    if missing:
        sys.stderr.write("regstub-lint: no such file: %s\n" % ", ".join(missing))
        return 2

    files, probe, literals = scan(paths, root)

    bad, gated = [], 0
    OPEN = ("if", "foreach", "while", "function", "macro")
    CLOSE = ("endif", "endforeach", "endwhile", "endfunction", "endmacro")
    for p, cmds in files.items():
        rel, stack, blocks, master = os.path.relpath(p, root), [], 0, None
        adopt, loops = [], []
        for name, ln, args in cmds:
            low = name.lower()
            if low in OPEN:
                blocks += 1
                if low == "if":
                    stack.append(Chain(ln, " ".join(args.split())))
                elif low in ("foreach", "while"):
                    loops.append(blocks)
            elif low == "elseif" and stack:
                stack[-1].branches.append({"kind": "elseif", "line": ln, "names": []})
            elif low == "else" and stack:
                stack[-1].branches.append({"kind": "else", "line": ln, "names": []})
            elif low in ("continue", "return") and stack:
                stack[-1].escaped = True
            elif low == "enable_testing" and stack:
                master = stack[-1]
            elif low in REG_CMDS and stack:
                toks = split_args(args)
                nm = toks[0] if toks else "?"
                if nm.upper() == "NAME" and len(toks) > 1:
                    nm = toks[1]
                if stack:
                    stack[-1].cur()["names"].append((nm, ln))
                for c, _ in adopt:
                    c.branches[-1]["names"].append((nm, ln))
            elif low in CLOSE:
                blocks -= 1
                if low == "endif" and stack:
                    c = stack.pop()
                    if any(b["names"] for b in c.branches) and c is not master:
                        why = gate_reason(c.cond, probe, literals)
                        if why is not None:
                            gated += 1
                            if c.escaped and not any(b["kind"] == "else"
                                                     for b in c.branches):
                                c.branches.append({"kind": "fallthrough",
                                                   "line": ln, "names": []})
                                adopt.append((c, loops[-1] if loops else 0))
                            else:
                                report(bad, rel, c, why)
                    for nm, nl in certain(c):
                        if stack:
                            stack[-1].cur()["names"].append((nm, nl))
                if low in ("endforeach", "endwhile") and loops:
                    loops.pop()
                for c, d in [x for x in adopt if x[1] > blocks]:
                    report(bad, rel, c, gate_reason(c.cond, probe, literals))
                adopt = [x for x in adopt if x[1] <= blocks]
        for c, _ in adopt:
            report(bad, rel, c, gate_reason(c.cond, probe, literals))

    if gated < a.min_chains:
        sys.stderr.write("regstub-lint: only %d capability-gated registration chain(s) "
                         "found, expected at least %d. Either the parser stopped early or "
                         "the probe-rooting analysis collapsed; a check that inspected "
                         "nothing must not be reported as passing\n" % (gated, a.min_chains))
        return 1
    for b in bad:
        print("regstub-lint: " + b)
    if bad:
        print("regstub-lint: %d of %d capability-gated registration chain(s) drop cells "
              "instead of skipping them" % (len(bad), gated))
        return 1
    print("regstub-lint: OK -- %d capability-gated registration chain(s), each registering "
          "the same cell names on every branch" % gated)
    return 0


def certain(c):
    """Names this chain registers no matter which branch runs."""
    if not any(b["kind"] in ("else", "fallthrough") for b in c.branches):
        return []
    keep, first = [], None
    for b in c.branches:
        s = set(n for n, _ in b["names"])
        first = s if first is None else (first & s)
    for b in c.branches:
        for n, l in b["names"]:
            if n in first and n not in [x for x, _ in keep]:
                keep.append((n, l))
    return keep


def report(bad, rel, c, why):
    branches = list(c.branches)
    if not any(b["kind"] in ("else", "fallthrough") for b in branches):
        branches.append({"kind": "absent-else", "line": c.line, "names": []})
    sets = [(b, set(n for n, _ in b["names"])) for b in branches]
    union = set()
    for _, s in sets:
        union |= s
    if all(s == union for _, s in sets):
        return
    if len(branches) == 2 and branches[-1]["kind"] == "absent-else":
        bad.append("%s:%d: if(%s) registers %s and has no else(). It is gated on %s, so "
                   "where that does not hold the cell is not registered at all -- invisible "
                   "to `ctest -N`, to tools/must-run.py and to every count in docs/TODO.md. "
                   "Add an else() that mcc_skip_test()s the same name with a reason"
                   % (rel, c.line, c.cond[:90], quote(sorted(union)), why))
        return
    for b, s in sets:
        gap = sorted(union - s)
        if gap:
            where = {"fallthrough": "the code it falls through to",
                     "absent-else": "its absent else()"}.get(
                b["kind"], "its %s branch at line %d" % (b["kind"], b["line"]))
            bad.append("%s:%d: if(%s) is gated on %s, and %s does not register %s. Every "
                       "branch of a capability gate registers the same cells; the ones it "
                       "cannot run it registers as a skip with a reason, under the same "
                       "name, so `ctest -N` counts the same either way"
                       % (rel, c.line, c.cond[:90], why, where, quote(gap)))
            return


def quote(names):
    if len(names) <= 3:
        return ", ".join("`%s`" % n for n in names)
    return "%s and %d more" % (", ".join("`%s`" % n for n in names[:3]), len(names) - 3)


if __name__ == "__main__":
    sys.exit(main())
