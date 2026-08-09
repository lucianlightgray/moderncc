#!/usr/bin/env python3
"""Census of printf-family call sites and format specifiers over a C corpus.

Regenerates the numbers docs/TODO.md board item 2 quotes. Default corpus is
src/*.c, matching docs/DEVICE-LIBC.md's rule that tests/exec must not be used
for the libc phase.

  tools/fmt-census.py                 # site census over src/*.c
  tools/fmt-census.py --json          # machine-readable
  tools/fmt-census.py --refused       # every site mcc_fmt_compile turns down
  tools/fmt-census.py path/to/*.c

fmt_compile below is a port of mcc_fmt_compile in src/mccfmt.h, including its
cost model. It is a second implementation and nothing gates the two against
each other automatically: slicerun's fmt_refusals pins the C side on the same
list of format spellings this port is checked against, and both lists have to
be edited together.

The block census, which is what the board's "+N blocks" figures mean, needs an
arena dump rather than source text:

  for f in src/*.c; do MCC_ARENA_DUMP=arenas.txt mcc -c -O1 -o /dev/null $f ...; done
  tools/fmt-census.py --arenas=arenas.txt
"""

import collections
import glob
import json
import os
import re
import sys

FUNCS = ("fprintf", "snprintf", "vsnprintf", "sprintf", "vfprintf", "printf",
         "vprintf", "asprintf", "dprintf")

SPEC = re.compile(r"%(?:[-+ #0]*)(?:\*|[0-9]+)?(?:\.(?:\*|[0-9]+))?"
                  r"(?:hh|h|ll|l|j|z|t|L)?([diouxXeEfFgGaAcspn%])")

T1 = {"d", "i", "u", "x", "X", "c"}
BLOCKED_PTR = {"s", "p"}
OUT_FLOAT = {"e", "E", "f", "F", "g", "G", "a", "A"}

MAXITEM, MAXARG, MAXW, MAXSTR = 24, 8, 32, 28
C_BASE, C_BYTE, C_DEC, C_HEX = 820, 152, 6900, 4700
C_SFIX, C_SBYTE, C_SDYN, MAXCOST = 130, 229, 14, 16384

OK, R_PTR, R_FLOAT, R_SPEC, R_ROOM = 0, 1, 2, 3, 4
WHY = {OK: "ok", R_PTR: "%p", R_FLOAT: "float", R_SPEC: "flag/width/conv",
       R_ROOM: "module budget"}


def fmt_compile(f):
    """Returns (verdict, cost). Verdict 0 is accepted."""
    items, narg, i, n = [], 0, 0, len(f)
    while i < n:
        if f[i] != "%":
            items.append(("L", 1)); i += 1; continue
        i += 1
        if i < n and f[i] == "%":
            items.append(("L", 1)); i += 1; continue
        w = zero = base = sgn = left = lmod = 0
        kind, pend, prc = "I", OK, -1
        while i < n:
            if f[i] == "-":
                left = 1; i += 1; continue
            if f[i] in "+ #":
                pend = R_SPEC; i += 1; continue
            if f[i] == "0":
                zero = 1; i += 1; continue
            break
        if i < n and f[i] == "*":
            pend = R_SPEC; i += 1
        else:
            while i < n and f[i].isdigit():
                w = w * 10 + int(f[i]); i += 1
                if w > MAXW:
                    pend = R_SPEC
        if i < n and f[i] == ".":
            i += 1
            if i < n and f[i] == "*":
                prc = -2; i += 1
            else:
                prc = 0
                while i < n and f[i].isdigit():
                    prc = min(prc * 10 + int(f[i]), MAXSTR); i += 1
        if f[i:i + 2] in ("hh", "ll"):
            lmod = 1; i += 2
        elif f[i:i + 1] in ("h", "l", "z", "t", "j"):
            lmod = 1; i += 1
        elif f[i:i + 1] == "L":
            return R_FLOAT, 0
        if i >= n:
            return R_SPEC, 0
        cv = f[i]
        if cv in "di":
            base, sgn = 10, 1
        elif cv == "u":
            base = 10
        elif cv in "xX":
            base = 16
        elif cv == "c":
            kind = "C"
        elif cv == "s":
            kind = "S"
        elif cv == "p":
            return R_PTR, 0
        elif cv in OUT_FLOAT:
            return R_FLOAT, 0
        else:
            return R_SPEC, 0
        i += 1
        if kind == "S":
            if pend or zero or lmod:
                pend = R_SPEC
        elif left or prc != -1 or (w and (sgn or kind == "C")):
            pend = R_SPEC
        if pend:
            return R_SPEC, 0
        if prc == -2:
            items.append(("P", 0)); narg += 1
        items.append((kind, base, w, prc)); narg += 1
        if narg > MAXARG or len(items) > MAXITEM:
            return R_ROOM, 0
    cost = C_BASE
    for it in items:
        if it[0] == "L":
            cost += C_BYTE * it[1]
        elif it[0] == "C":
            cost += C_BYTE
        elif it[0] == "P":
            pass
        elif it[0] == "S":
            nb = it[3] if 0 <= it[3] < MAXSTR else MAXSTR
            cost += C_SFIX + nb * (C_SBYTE + (C_SDYN if it[3] == -2 else 0)) \
                + it[2] * C_BYTE
        else:
            cost += (C_DEC if it[1] == 10 else C_HEX) + it[2] * C_BYTE
    if cost > MAXCOST:
        return R_ROOM, cost
    return OK, cost


def strip(src):
    """Remove comments; keep string literals (we need them)."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < n:
                if src[j] == "\\":
                    j += 2
                    continue
                if src[j] == c:
                    j += 1
                    break
                j += 1
            out.append(src[i:j])
            i = j
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(" " * (j - i))
            i = j
            continue
        if src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
            continue
        out.append(c)
        i += 1
    return "".join(out)


def args(src, k):
    """Split the argument list starting at the '(' at index k. Returns
    (list-of-arg-strings, index-past-close) or (None, k)."""
    depth, i, n, start, parts = 0, k, len(src), k + 1, []
    while i < n:
        c = src[i]
        if c == '"' or c == "'":
            i += 1
            while i < n:
                if src[i] == "\\":
                    i += 2
                    continue
                if src[i] == c:
                    break
                i += 1
            i += 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                parts.append(src[start:i])
                return parts, i + 1
        elif c == "," and depth == 1:
            parts.append(src[start:i])
            start = i + 1
        i += 1
    return None, k


LIT = re.compile(r'^\s*(?:"(?:[^"\\]|\\.)*"\s*)+$')
PIECE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def literal(a):
    if not LIT.match(a):
        return None
    return "".join(PIECE.findall(a))


def fmt_index(fn):
    return {"fprintf": 1, "vfprintf": 1, "snprintf": 2, "vsnprintf": 2,
            "sprintf": 1, "printf": 0, "vprintf": 0, "asprintf": 1,
            "dprintf": 1}.get(fn, 0)


AST_BB = 0
AST_INVOKE = 11
AST_NONE = 0xFFFFFFFF
SNFAM = ("snprintf", "vsnprintf", "sprintf")


def arena_blocks(path):
    """Re-derive the board's block figure from MCC_ARENA_DUMP output.

    A block is a non-empty AST_BasicBlock; it is Invoke-blocked if its subtree
    contains an AST_Invoke. Adding one callee unblocks a block only when every
    invoke in that block names a callee in the set, which is why the per-callee
    numbers do not sum."""
    tot = collections.Counter()
    per = collections.Counter()
    kind, fc, ns, inv, arenas = {}, {}, {}, {}, []
    cur = None

    def flush():
        if cur is None:
            return
        k, f, s, iv = cur
        kids = {}
        for n in k:
            c = f.get(n, AST_NONE)
            while c in k:
                kids.setdefault(n, []).append(c)
                c = s.get(c, AST_NONE)

        memo = {}

        def callees(n):
            if n in memo:
                return memo[n]
            r = set()
            if k.get(n) == AST_INVOKE:
                r.add(iv.get(n, "?"))
            for c in kids.get(n, ()):
                r |= callees(c)
            memo[n] = r
            return r

        for n in k:
            if k.get(n) != AST_BB or not kids.get(n):
                continue
            tot["blocks"] += 1
            cs = callees(n)
            if not cs:
                continue
            tot["invoke_blocked"] += 1
            for name in cs:
                per[name] += 0
            for name in set(cs):
                if cs <= {name}:
                    per[name] += 1
            if cs and cs <= set(SNFAM):
                tot["snprintf_only"] += 1

    with open(path, "r", errors="replace") as fh:
        for line in fh:
            if line.startswith("[arena]"):
                flush()
                kind, fc, ns, inv = {}, {}, {}, {}
                cur = (kind, fc, ns, inv)
                arenas.append(1)
                continue
            if cur is None:
                continue
            if line.startswith("[inv]"):
                p = line.split()
                if len(p) >= 3:
                    inv[int(p[1])] = p[2]
                continue
            p = line.split()
            if len(p) < 7:
                continue
            try:
                n = int(p[0])
            except ValueError:
                continue
            kind[n] = int(p[1])
            fc[n] = int(p[5])
            ns[n] = int(p[6])
    flush()
    return len(arenas), tot, per


def main(argv):
    as_json = "--json" in argv
    for a in argv:
        if a.startswith("--arenas="):
            na, tot, per = arena_blocks(a.split("=", 1)[1])
            print("arenas: %d" % na)
            print("non-empty AST_BasicBlock: %d" % tot["blocks"])
            print("Invoke-blocked: %d" % tot["invoke_blocked"])
            print("unblocked by the snprintf family alone: %d" %
                  tot["snprintf_only"])
            print("\ntop single-callee unblocks:")
            for name, c in per.most_common(20):
                if c:
                    print("  %-24s %5d" % (name, c))
            return 0
    paths = [a for a in argv if not a.startswith("--")]
    if not paths:
        root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        paths = sorted(glob.glob(os.path.join(root, "src", "*.c")))

    sites = collections.Counter()
    lit_sites = collections.Counter()
    specs = collections.Counter()
    flagged = collections.Counter()
    ret_used = collections.Counter()
    tu = set()
    sn_class = collections.Counter()
    verdict = collections.Counter()
    refused = []
    maxcost = [0]

    call = re.compile(r"\b(" + "|".join(FUNCS) + r")\s*\(")
    for p in paths:
        try:
            with open(p, "r", errors="replace") as f:
                src = f.read()
        except OSError:
            continue
        src = strip(src)
        tu.add(p)
        for m in call.finditer(src):
            fn = m.group(1)
            if m.start() and (src[m.start() - 1].isalnum() or
                              src[m.start() - 1] == "_"):
                continue
            al, end = args(src, m.end() - 1)
            if al is None:
                continue
            sites[fn] += 1
            fi = fmt_index(fn)
            f = literal(al[fi]) if fi < len(al) else None
            if f is None:
                continue
            lit_sites[fn] += 1
            pre = src[max(0, m.start() - 40):m.start()]
            used = not re.search(r"[;{}\)]\s*$|^\s*$", pre) or \
                bool(re.search(r"(\+=|=|<|>|\breturn\b|\bif\b|\bwhile\b)\s*$",
                               pre.rstrip()))
            if used:
                ret_used[fn] += 1
            site_kinds = set()
            for sm in SPEC.finditer(f):
                conv = sm.group(1)
                if conv == "%":
                    continue
                body = sm.group(0)[1:-1]
                length = ""
                for L in ("hh", "ll", "h", "l", "j", "z", "t", "L"):
                    if body.endswith(L):
                        length = L
                        body = body[:-len(L)]
                        break
                if fn == "snprintf":
                    specs[length + conv] += 1
                    if body:
                        flagged[sm.group(0)] += 1
                    site_kinds.add(conv)
            if fn == "snprintf":
                if not site_kinds:
                    sn_class["literal-only"] += 1
                elif site_kinds & BLOCKED_PTR:
                    sn_class["blocked-on-pointer"] += 1
                elif site_kinds & OUT_FLOAT:
                    sn_class["float-out-of-scope"] += 1
                elif site_kinds <= T1:
                    sn_class["tranche1"] += 1
                else:
                    sn_class["other"] += 1
                v, cost = fmt_compile(f)
                verdict[WHY[v]] += 1
                if v == OK:
                    maxcost[0] = max(maxcost[0], cost)
                    if "s" in site_kinds:
                        verdict["  of which carry a %s"] += 1
                else:
                    refused.append((WHY[v], f, p))

    tot = sum(specs.values())
    rep = {
        "tus": len(tu),
        "sites": dict(sites),
        "literal_fmt_sites": dict(lit_sites),
        "return_consumed": dict(ret_used),
        "snprintf_specifiers_total": tot,
        "snprintf_specifiers": dict(specs.most_common()),
        "snprintf_flagged_specs": dict(flagged.most_common()),
        "snprintf_site_class": dict(sn_class),
        "snprintf_compile_verdict": dict(verdict),
        "max_accepted_cost": maxcost[0],
        "refused": [{"why": w, "fmt": f, "file": p} for w, f, p in refused],
    }
    if as_json:
        print(json.dumps(rep, indent=2))
        return 0
    print("TUs: %d" % len(tu))
    print("\ncall sites (literal fmt):")
    for fn, c in sites.most_common():
        print("  %-10s %4d  (%d)" % (fn, c, lit_sites[fn]))
    print("\nsnprintf return consumed at %d/%d literal sites" %
          (ret_used["snprintf"], lit_sites["snprintf"]))
    print("\nsnprintf specifiers, %d total:" % tot)
    for s, c in specs.most_common():
        print("  %%%-5s %4d  %5.1f%%" % (s, c, 100.0 * c / max(tot, 1)))
    print("\nsnprintf flags/width/precision:")
    for s, c in flagged.most_common():
        print("  %-8s %3d" % (s, c))
    print("\nsnprintf sites by tranche:")
    for k, c in sn_class.most_common():
        print("  %-20s %4d  %5.1f%%" % (k, c, 100.0 * c /
                                        max(lit_sites["snprintf"], 1)))
    ns = max(lit_sites["snprintf"], 1)
    print("\nmcc_fmt_compile verdict, MCC_FMT_MAXSTR=%d, budget %d words:" %
          (MAXSTR, MAXCOST))
    for k, c in verdict.most_common():
        print("  %-24s %4d  %5.1f%%" % (k, c, 100.0 * c / ns))
    print("  largest accepted program: %d words" % maxcost[0])
    if "--refused" in argv:
        print("\nrefused sites:")
        for w, f, p in refused:
            print("  %-16s %-64r %s" % (w, f, os.path.basename(p)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
