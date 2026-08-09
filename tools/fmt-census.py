#!/usr/bin/env python3
"""Census of printf-family call sites and format specifiers over a C corpus.

Regenerates the numbers docs/TODO.md board item 2 quotes. Default corpus is
src/*.c, matching docs/DEVICE-LIBC.md's rule that tests/exec must not be used
for the libc phase.

  tools/fmt-census.py                 # site census over src/*.c
  tools/fmt-census.py --json          # machine-readable
  tools/fmt-census.py --refused       # every site mcc_fmt_compile turns down
  tools/fmt-census.py --check         # gate the census against tests/fmt/census-bank.json
  tools/fmt-census.py --update-bank   # re-take that bank
  tools/fmt-census.py path/to/*.c

THE CORPUS, because a glob is a silent denominator. src/*.c is not eighteen
independent translation units. src/libmcc.c #includes fifteen of its siblings
and src/mcc.c #includes libmcc.c and mcctools.c, so exactly one of the eighteen
is a real TU; the rest are textual fragments. That does not affect this census,
which scans each file's own text once and never follows an #include -- no body
is counted twice -- but it does affect the arena loop at the bottom of this
docstring, which compiles what it is given, so a loop over src/*.c compiles
most of the tree seventeen times over. That loop names its files explicitly for
that reason.

The roster is pinned in CORPUS below rather than globbed. A glob answers
"whatever is on disk", so adding, renaming or deleting a src/*.c silently moves
every printed figure and the board's row goes stale with no signal; --check
compares the roster against the disk and fails on any difference before it
compares a single number. Two of the eighteen -- mccdbg.c and mccjit_intent.c --
hold no printf-family call site at all, which is why "TUs:" reads 16 and not 18;
they are listed anyway and --check pins their per-file counts, so a file that
stops contributing is a failure rather than a quietly smaller denominator.

fmt_compile below is a port of mcc_fmt_compile in src/mccfmt.h, including its
cost model. It is a second implementation, so it is gated:

  tools/fmt-census.py --selfcheck --oracle=<path-to-slicerun>

runs every corpus format plus a generated set through `slicerun --fmt-verdict`,
which calls the real mcc_fmt_compile, and fails on the first disagreement. The
ctest cell fmt/census-oracle runs exactly that, and fmt/census-oracle-known-
positive proves the check can fail. Pass --oracle without --selfcheck to have
the census report the compiler's own verdicts rather than the port's.

The port drifted once already: it appended one item per literal byte instead of
merging runs the way mcc_fmt_lit does, so any format with more than 24 leading
literal characters was reported "module budget" when the compiler accepted it.
That cost the board two sites (140/162 reported, 142/162 real).

The block census, which is what the board's "+N blocks" figures mean, needs an
arena dump rather than source text. This loop must NOT be `for f in src/*.c`:
mcc.c pulls in libmcc.c which pulls in fifteen more, so that spelling compiles
most of the tree seventeen times and the dump double-counts nearly every block.
Compile the one real TU, and only it:

  MCC_ARENA_DUMP=arenas.txt mcc -c -O1 -o /dev/null src/mcc.c ...
  tools/fmt-census.py --arenas=arenas.txt

The 8,250-arena / 56,281-block figure the board quotes against `src/*.c` was
taken with the loop spelling, so it counts most bodies many times over. It is
NOT re-measured here and --check does not cover it: the arena path needs a live
recorder, not source text. Treat that row as an upper bound of unknown
tightness until someone re-takes it; the board says so too.
"""

import collections
import glob
import json
import os
import random
import re
import subprocess
import sys

CORPUS = ("src/libmcc.c", "src/mcc.c", "src/mccasm.c", "src/mccast.c",
          "src/mcccst.c", "src/mccdbg.c", "src/mccdis.c", "src/mccgen.c",
          "src/mccgpu.c", "src/mcchost.c", "src/mccircap.c",
          "src/mccjit_embed.c", "src/mccjit_intent.c", "src/mccpp.c",
          "src/mccrir.c", "src/mccrun.c", "src/mccstats.c", "src/mcctools.c")

FUNCS = ("fprintf", "snprintf", "vsnprintf", "sprintf", "vfprintf", "printf",
         "vprintf", "asprintf", "dprintf")

SPEC = re.compile(r"%(?:[-+ #0]*)(?:\*|[0-9]+)?(?:\.(?:\*|[0-9]+))?"
                  r"(?:hh|h|ll|l|j|z|t|L)?([diouxXeEfFgGaAcspn%])")

T1 = {"d", "i", "u", "x", "X", "c"}
BLOCKED_PTR = {"s", "p"}
OUT_FLOAT = {"e", "E", "f", "F", "g", "G", "a", "A"}

MAXITEM, MAXARG, MAXW, MAXSTR, MAXLIT = 24, 8, 32, 28, 192
C_BASE, C_BYTE, C_HEX = 820, 152, 4700
C_DEC, C_DEC32, C_HEX32 = 6900, 2400, 1750
C_SFIX, C_SBYTE, C_SDYN, MAXCOST = 130, 229, 14, 16384

OK, R_PTR, R_FLOAT, R_SPEC, R_ROOM = 0, 1, 2, 3, 4
WHY = {OK: "ok", R_PTR: "%p", R_FLOAT: "float", R_SPEC: "flag/width/conv",
       R_ROOM: "module budget"}

MUTATE = [0]


def fmt_compile(f):
    """Returns (verdict, cost). Verdict 0 is accepted."""
    items, narg, nlit, i, n = [], 0, 0, 0, len(f)

    def lit():
        nonlocal nlit
        if nlit >= MAXLIT:
            return False
        if items and items[-1][0] == "L" and not MUTATE[0]:
            items[-1][1] += 1
            nlit += 1
            return True
        if len(items) >= MAXITEM:
            return False
        items.append(["L", 1])
        nlit += 1
        return True

    while i < n:
        if f[i] != "%":
            i += 1
            if not lit():
                return R_ROOM, 0
            continue
        i += 1
        if i < n and f[i] == "%":
            i += 1
            if not lit():
                return R_ROOM, 0
            continue
        w = zero = base = sgn = wide = left = lmod = 0
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
            lmod = 1; wide = 1 if f[i] == "l" else 0; i += 2
        elif f[i:i + 1] == "h":
            lmod = 1; i += 1
        elif f[i:i + 1] in ("l", "z", "t", "j"):
            lmod = wide = 1; i += 1
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
            if len(items) >= MAXITEM or narg >= MAXARG:
                return R_ROOM, 0
            items.append(["P", 0]); narg += 1
        if len(items) >= MAXITEM or narg >= MAXARG:
            return R_ROOM, 0
        items.append([kind, base, w, prc, wide]); narg += 1
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
        elif it[1] == 10:
            cost += (C_DEC if it[4] else C_DEC32) + it[2] * C_BYTE
        else:
            cost += (C_HEX if it[4] else C_HEX32) + it[2] * C_BYTE
    if cost > MAXCOST:
        return R_ROOM, cost
    return OK, cost


ESC = {"n": "\n", "t": "\t", "r": "\r", "a": "\a", "b": "\b", "f": "\f",
       "v": "\v", "e": "\x1b", "\\": "\\", '"': '"', "'": "'", "?": "?"}


def unescape(s):
    """Decode the C escapes in a string literal's text. The compiler sees the
    decoded bytes, so costing "\\n" as two literal bytes overcharges the site."""
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c != "\\" or i + 1 >= n:
            out.append(c); i += 1; continue
        d = s[i + 1]
        if d in ESC:
            out.append(ESC[d]); i += 2; continue
        if d == "x":
            j = i + 2
            while j < n and s[j] in "0123456789abcdefABCDEF":
                j += 1
            if j > i + 2:
                out.append(chr(int(s[i + 2:j], 16) & 0xFF)); i = j; continue
            out.append(d); i += 2; continue
        if d in "01234567":
            j, v = i + 1, 0
            while j < n and j < i + 4 and s[j] in "01234567":
                v = v * 8 + int(s[j]); j += 1
            out.append(chr(v & 0xFF)); i = j; continue
        out.append(d); i += 2
    return "".join(out)


def oracle_verdicts(path, fmts):
    """Ask the compiler itself. Returns [(verdict, cost, nitem, narg)]."""
    inp = "".join(f.encode("latin-1", "replace").hex() + "\n" for f in fmts)
    r = subprocess.run([path, "--fmt-verdict"], input=inp, capture_output=True,
                       text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        raise SystemExit("oracle %s failed rc=%d" % (path, r.returncode))
    rows = [tuple(int(x) for x in l.split()) for l in r.stdout.splitlines()]
    if len(rows) != len(fmts):
        raise SystemExit("oracle returned %d rows for %d formats" %
                         (len(rows), len(fmts)))
    return rows


GEN_LIT = "abcdefg ,:.=/[]()<>-_\n\t"
GEN_CONV = ("%d", "%u", "%s", "%c", "%x", "%X", "%lld", "%llu", "%llx", "%lu",
            "%zu", "%ld", "%.*s", "%02x", "%08x", "%016llx", "%-8s", "%6s",
            "%.5s", "%%", "%2d", "%-10d", "%p", "%f", "%.17g", "%o", "%n",
            "%+d", "%#x", "%*d", "%40s", "%hhd", "%hd", "%33d", "%.3d")


def generated(n=40000):
    """A deterministic corpus that walks the item, argument, literal-run and
    budget limits, which is where the port drifted before."""
    rnd = random.Random(20260808)
    out = ["", "%", "%%", "%l", "%.", "%.*", "%1", "%-", "%hh"]
    for k in (1, 23, 24, 25, 191, 192, 193, 200):
        out.append("a" * k)
        out.append("a" * k + "%d")
        out.append("%d" + "a" * k)
    for _ in range(n):
        parts = []
        for _ in range(rnd.randint(0, 40)):
            if rnd.random() < 0.7:
                parts.append(rnd.choice(GEN_LIT))
            else:
                parts.append(rnd.choice(GEN_CONV))
        out.append("".join(parts))
    return out


def selfcheck(oracle, paths):
    fmts = [f for _, f, _, _ in call_sites(paths) if f is not None]
    fmts = [f for f in fmts + generated() if "\0" not in f]
    rows = oracle_verdicts(oracle, fmts)
    bad = 0
    for f, (v, cost, nitem, narg) in zip(fmts, rows):
        pv, pc = fmt_compile(f)
        if pv != v or (v == OK and pc != cost):
            bad += 1
            if bad <= 20:
                print("DRIFT port=(%s,%d) mcc_fmt_compile=(%s,%d) %r" %
                      (WHY[pv], pc, WHY[v], cost, f))
    print("fmt-census selfcheck: %d formats, %d disagreements" %
          (len(fmts), bad))
    if not fmts or len(fmts) < 1000:
        print("FAIL: the selfcheck corpus is too small to mean anything")
        return 1
    return 1 if bad else 0


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
    return unescape("".join(PIECE.findall(a)))


CALL = re.compile(r"\b(" + "|".join(FUNCS) + r")\s*\(")


def call_sites(paths):
    """Yields (func, decoded-format-or-None, path, preceding-40-chars)."""
    for p in paths:
        try:
            with open(p, "r", errors="replace") as f:
                src = f.read()
        except OSError:
            continue
        src = strip(src)
        for m in CALL.finditer(src):
            fn = m.group(1)
            if m.start() and (src[m.start() - 1].isalnum() or
                              src[m.start() - 1] == "_"):
                continue
            al, end = args(src, m.end() - 1)
            if al is None:
                continue
            fi = fmt_index(fn)
            f = literal(al[fi]) if fi < len(al) else None
            yield fn, f, p, src[max(0, m.start() - 40):m.start()]


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


def corpus_paths(root):
    """The pinned roster, checked against the disk. Both directions are fatal:
    a listed file that is gone shrinks every figure, and a src/*.c that nobody
    listed is a body the census silently does not see."""
    want = [os.path.join(root, p) for p in CORPUS]
    missing = [p for p in CORPUS if not os.path.exists(os.path.join(root, p))]
    on_disk = set(glob.glob(os.path.join(root, "src", "*.c")))
    unlisted = sorted(os.path.relpath(p, root) for p in on_disk - set(want))
    if missing or unlisted:
        for p in missing:
            sys.stderr.write("fmt-census: CORPUS lists %s, which is not on "
                             "disk\n" % p)
        for p in unlisted:
            sys.stderr.write("fmt-census: %s is on disk and not in CORPUS, so "
                             "every figure below would be taken over a corpus "
                             "nobody declared\n" % p)
        raise SystemExit("fmt-census: the corpus moved under the census; "
                         "update CORPUS and re-take the bank with "
                         "--update-bank, saying so on the board")
    return want


def census_key(rep):
    """The figures the board quotes, and only those, so the bank is a ratchet
    and not a diff of the whole report."""
    v = rep["snprintf_compile_verdict"]
    return {
        "tus": rep["tus"],
        "sites": rep["sites"],
        "literal_fmt_sites": rep["literal_fmt_sites"],
        "per_file_sites": rep["per_file_sites"],
        "snprintf_return_consumed": rep["return_consumed"].get("snprintf", 0),
        "snprintf_specifiers_total": rep["snprintf_specifiers_total"],
        "snprintf_site_class": rep["snprintf_site_class"],
        "accepted": v.get("ok", 0),
        "accepted_with_s": v.get("  of which carry a %s", 0),
        "refused_budget": v.get("module budget", 0),
        "refused_flag": v.get("flag/width/conv", 0),
        "refused_float": v.get("float", 0),
        "refused_ptr": v.get("%p", 0),
        "max_accepted_cost": rep["max_accepted_cost"],
    }


BANK = "tests/fmt/census-bank.json"


def check_bank(root, rep, update):
    path = os.path.join(root, BANK)
    now = census_key(rep)
    if now["literal_fmt_sites"].get("snprintf", 0) < 100:
        print("FAIL: %d literal snprintf sites is below the floor of 100 -- "
              "this census has no subject and its percentages mean nothing" %
              now["literal_fmt_sites"].get("snprintf", 0))
        return 1
    if update:
        with open(path, "w") as f:
            json.dump(now, f, indent=1, sort_keys=True)
            f.write("\n")
        print("fmt-census: banked %s" % BANK)
        return 0
    if not os.path.exists(path):
        print("FAIL: no bank at %s. A missing bank is a failure, not a pass -- "
              "there is nothing here for the census to be checked against" %
              BANK)
        return 1
    with open(path) as f:
        was = json.load(f)
    bad = []
    for k in sorted(set(was) | set(now)):
        if was.get(k) != now.get(k):
            bad.append("  %-26s banked %r, now %r" % (k, was.get(k),
                                                     now.get(k)))
    if bad:
        print("FAIL: the site census moved against %s:" % BANK)
        print("\n".join(bad))
        print("\nEvery one of these is quoted on the board. Re-take with "
              "--update-bank and say on the board which figure moved and why.")
        return 1
    print("fmt-census: OK, %d pinned figures over %d files, %d literal "
          "snprintf sites, %d accepted" %
          (len(now), len(CORPUS), now["literal_fmt_sites"]["snprintf"],
           now["accepted"]))
    return 0


def main(argv):
    as_json = "--json" in argv
    oracle = None
    for a in argv:
        if a.startswith("--oracle="):
            oracle = a.split("=", 1)[1]
        elif a == "--mutate":
            MUTATE[0] = 1
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
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for a in argv:
        if a.startswith("--root="):
            root = a.split("=", 1)[1]
    paths = [a for a in argv if not a.startswith("--")]
    pinned = not paths
    if pinned:
        paths = corpus_paths(root)

    if "--selfcheck" in argv:
        if not oracle:
            raise SystemExit("--selfcheck needs --oracle=<path-to-slicerun>")
        return selfcheck(oracle, paths)

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

    found = list(call_sites(paths))
    sn = [(f, p) for fn, f, p, _ in found if fn == "snprintf" and f is not None]
    if oracle:
        rows = oracle_verdicts(oracle, [f for f, _ in sn])
        oracled = {i: (rows[i][0], rows[i][1]) for i in range(len(rows))}
    else:
        oracled = None
    per_file = collections.Counter()
    sni = 0
    for fn, f, p, pre in found:
        tu.add(p)
        per_file[os.path.basename(p)] += 1
        sites[fn] += 1
        if f is None:
            continue
        lit_sites[fn] += 1
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
            v, cost = oracled[sni] if oracled else fmt_compile(f)
            sni += 1
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
        "per_file_sites": {os.path.basename(p): per_file.get(
            os.path.basename(p), 0) for p in paths},
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
    if "--check" in argv or "--update-bank" in argv:
        if not pinned:
            raise SystemExit("fmt-census: --check gates the pinned CORPUS; it "
                             "means nothing over an ad-hoc file list")
        return check_bank(root, rep, "--update-bank" in argv)
    if as_json:
        print(json.dumps(rep, indent=2))
        return 0
    print("corpus: %d file(s)%s" %
          (len(paths), ", pinned in CORPUS" if pinned else ", given on argv"))
    print("TUs: %d  (a file with no printf-family call site is not a TU here)"
          % len(tu))
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
    print("\nmcc_fmt_compile verdict (%s), MCC_FMT_MAXSTR=%d, budget %d words:" %
          ("oracle" if oracle else "port", MAXSTR, MAXCOST))
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
