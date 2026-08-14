#!/usr/bin/env python3
"""Census of maximal call-free slices in the RIR production arena.

A *slice* here is the unit a GPU compute shader could plausibly consume: a
maximal run of consecutive statements, inside one statement list of a function
body, whose subtrees contain no `AST_Invoke` the arena cannot see through.
Everything about that definition is load-bearing:

  maximal      a slice cannot be grown by absorbing an adjacent sibling
               statement (the neighbour has a call) nor by moving up to the
               parent statement list (the parent statement has a call).  When a
               whole loop is call-free the slice is the loop, nest and all --
               loops are not split out, they are ordinary statements.
  statements   not expressions.  A call-free *fragment* of a call-containing
               expression (`a[i] + h(s,i)` minus the `h`) is not a slice; that
               is what the pre-existing expression-window machinery in
               src/mccast.c (ast_slice_ident_hash / ast_slice_win_root_ok /
               -fopt-slice) already enumerates, and those windows are 3..64
               nodes of pure integer arithmetic.
  see through  two partitions are reported side by side.
                 t=0  every AST_Invoke is a boundary.
                 t=1  an AST_Invoke whose callee Sym is in the graft-inline
                      pool with graftable=1 is *transparent*: at that -O level
                      the arena replays the callee's body in place, so the
                      call is not a real boundary.  t=1 minus t=0 is exactly
                      the merging effect of inlining.
               Indirect calls (callee is not a Ref to a VT_FUNC Sym) can never
               be transparent; neither can direct calls to symbols with no body
               in this TU.  Those are the "truly anonymous" invokes.

Byte extents are exact, not modelled: src/mccast.c brackets each statement's
replay in ast_replay_bb with the code index `ind`, so every top-level statement
of a slice carries the byte count it emitted during the faithfulness replay.
`attr` vs `bytes` on the per-function line is the reconciliation: attributed
statement bytes against the parser's body length.

Compiler side is the `MCC_SLICE_CENSUS=<path|->` env var, which appends

    [slice]    fn= t= id= stmts= nodes= bytes= depth= loops= kinds=
    [slice-fn] fn= nodes= bytes= attr= stmts= faithful= loops= inv_*= ns/nb/nn=

one `[slice]` per slice and one `[slice-fn]` per modelled body.  Bodies the
arena never modelled emit nothing, so `fn_n` here is the modelled population,
not every function compiled.

Corpus.  A `tests/exec` source is not a free-standing translation unit: what it
is, and whether it is compilable on this target at all, is declared by its row
in `tests/exec/goldens.h`, which is the same table `tests/runner.c` executes.
Walking the directory and handing every `.c` to a bare `mcc -c` therefore
mis-states the corpus in two directions at once -- it drops sources that only
need the flags their row already names (`-trigraphs`, `-std=gnu89`,
`-std=c2y -pedantic-errors`, `-Itests/support`), and it counts as failures
sources the table says this target cannot build (`cpu=arm64`, `cpu=i386`,
`os=WIN32`) or that mcc rejects on purpose (`note:` rows such as whole-array
assignment).  Both are read here: `flags` is passed, minus the link-only
entries that `-c` refuses, and `req` is honoured by the same rules as
`req_met()` in tests/runner.c, restricted to the clauses that decide whether a
*compile* can happen -- the run-time gates (`asm`, `bcheck`, `backtrace`,
`diff3!=`) do not, because this census never runs the program.  Sources
excluded that way are named in the report, so the denominator stays auditable;
a source that fails to compile is still a hard failure, never a silent drop.

Usage:
  tools/slice-census.py <build-dir> [--levels O0,O1,O2,O3]
                        [--corpus self|exec|wide] [--sources ...]
                        [--top N] [--json FILE] [--jobs N] [--opt-in]
"""
import argparse, ast, json, os, re, shlex, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

KINDS = [
    ("store", 0), ("load", 1), ("binary", 2), ("unary", 3), ("convert", 4),
    ("literal", 5), ("ref", 6), ("jump", 7), ("return", 8), ("storeval", 9),
    ("poison", 10), ("bb", 11), ("if", 12), ("while", 13), ("for", 14),
    ("do", 15), ("switch", 16), ("ternary", 17), ("invoke", 18), ("asm", 19),
    ("float", 20),
]
LOOPY = (1 << 13) | (1 << 14) | (1 << 15)
BUCKETS = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]


def find_mcc(bdir):
    mcc = os.path.join(bdir, "mcc")
    if not os.access(mcc, os.X_OK) and os.access(mcc + ".exe", os.X_OK):
        mcc += ".exe"
    if not os.access(mcc, os.X_OK):
        sys.exit("slice-census: no mcc at " + mcc)
    return mcc


def self_flags(bdir):
    cdb = os.path.join(bdir, "compile_commands.json")
    if not os.path.exists(cdb):
        return []
    cc = json.load(open(cdb))
    rec = [x for x in cc if x["file"].endswith("/mcc.c")]
    if not rec:
        return []
    cmd = rec[0]["command"]
    # On Windows the command carries backslash paths (-IC:\...); POSIX shlex
    # eats them as escapes and the -I paths vanish, so src/mcc.c fails to find
    # libmcc.h. Turn path backslashes into forward slashes, leaving \" escapes.
    if os.name == "nt":
        cmd = re.sub(r'\\(?!")', "/", cmd)
    return [a for a in shlex.split(cmd)[1:]
            if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]


def read_goldens():
    """tests/exec/goldens.h as {abs source path: (flags, req)}.

    The first row wins when several goldens share one source, so the mapping is
    a function of the table's order and not of the walk's.
    """
    path = os.path.join(ROOT, "tests", "exec", "goldens.h")
    out = {}
    if not os.path.exists(path):
        return out
    txt = open(path, errors="replace").read()
    for row in re.findall(r'^\s*\{(".*")\s*\},\s*$', txt, re.M):
        try:
            rec = ast.literal_eval("(" + row + ")")
        except (SyntaxError, ValueError):
            continue
        if len(rec) < 7:
            continue
        key = os.path.normpath(os.path.join(ROOT, "tests", rec[2]))
        out.setdefault(key, (rec[4], rec[6]))
    return out


def target_id(mcc):
    """(cpu, os) of the target `mcc` builds for, in tests/runner.c's spelling."""
    cpu = os.environ.get("MCC_TEST_CPU")
    tos = os.environ.get("MCC_TEST_OS")
    if cpu and tos:
        return cpu, tos
    try:
        p = subprocess.run([mcc, "--version"], capture_output=True, text=True)
    except OSError:
        return cpu or "unknown", tos or "unknown"
    m = re.search(r"\(([^()\s]+)\s+([^()]+)\)", (p.stdout or "") + (p.stderr or ""))
    if m:
        cpu = cpu or m.group(1)
        tos = tos or m.group(2).strip()
    return cpu or "unknown", tos or "unknown"


def req_unmet(req, cpu, tos):
    """Why goldens.h says this source cannot be compiled here, or None.

    A port of the compile-deciding half of req_met() in tests/runner.c.  The
    clauses left out -- `asm`, `bcheck`, `backtrace`, `diff3!=` -- gate running
    or differential comparison, not translation, and a census that only ever
    reaches `-c` must not drop a source over them.
    """
    if not req:
        return None
    for tok in req.split(","):
        tok = tok.strip()
        if tok.startswith("note:"):
            return tok[5:]
        if tok.startswith("cpu="):
            want = tok[4:]
            ok = cpu in ("i386", "x86_64") if want == "x86" else cpu == want
            if not ok:
                return "requires %s target (host target: %s)" % (want, cpu)
        elif tok.startswith("os!="):
            want, _, why = tok[4:].partition(":")
            if tos == want:
                return why or ("not applicable to the %s target" % want)
        elif tok.startswith("os="):
            if tos != tok[3:]:
                return "requires %s target OS (host: %s)" % (tok[3:], tos)
        elif tok == "elf":
            if tos in ("Darwin", "WIN32"):
                return "requires an ELF target (host: %s)" % tos
        elif tok.startswith("skipon="):
            plat, _, why = tok[7:].partition(":")
            wcpu, slash, wos = plat.partition("/")
            if slash and cpu == wcpu and tos == wos:
                return why or ("not run on %s/%s" % (wcpu, wos))
    return None


def golden_flags(flags):
    """The golden's flags a `-c` compile can take: mcc refuses `-l` with -c."""
    return [f for f in shlex.split(flags or "")
            if not f.startswith("-l") and not f.startswith("-L")]


def needs_x11(path):
    """True if this source includes <X11/Xlib.h>.

    The `wide` corpus walks examples/ as well as tests/, and goldens.h cannot
    speak for a file that is not a golden -- so the X11 examples had no way to
    be excluded and no way to be given an include path either.  CMakeLists.txt
    already resolves MCC_X11_INCLUDE_DIR for exactly these files and hands the
    compile.ex* cells either the -I or a skip; this reads the same variable so
    the two agree instead of one of them silently failing to compile a source
    the other builds.
    """
    try:
        with open(path, encoding="utf8", errors="replace") as f:
            return "X11/Xlib.h" in f.read()
    except OSError:
        return False


def annotate(paths, gold, cpu, tos, drop):
    """[(path, extra flags)] plus the [(path, why)] goldens.h excludes."""
    tests = os.path.join(ROOT, "tests") + os.sep
    inc = ["-I" + os.path.join(ROOT, "runtime", "include"),
           "-I" + os.path.join(ROOT, "tests", "support")]
    x11 = os.environ.get("MCC_X11_INCLUDE_DIR", "")
    out, dropped = [], []
    for p in paths:
        key = os.path.normpath(os.path.abspath(p))
        extra = list(inc) if key.startswith(tests) else []
        rec = gold.get(key)
        if rec:
            why = req_unmet(rec[1], cpu, tos)
            if why and drop:
                dropped.append((os.path.relpath(key, ROOT), why))
                continue
            extra += golden_flags(rec[0])
        elif needs_x11(key):
            if not x11:
                if drop:
                    dropped.append((os.path.relpath(key, ROOT),
                                    "X11 example; <X11/Xlib.h> not available on "
                                    "this host/target"))
                    continue
            else:
                extra.append("-I" + x11)
        out.append((p, extra))
    return out, dropped


def bucket(n):
    for i, b in enumerate(BUCKETS):
        if n <= b:
            return i
    return len(BUCKETS)


def bucket_names():
    out, lo = [], 1
    for b in BUCKETS:
        out.append(str(lo) if lo == b else "%d-%d" % (lo, b))
        lo = b + 1
    out.append("%d+" % lo)
    return out


def new_part():
    return {"n": 0, "nodes": 0, "bytes": 0, "stmts": 0,
            "hist_nodes": [0] * (len(BUCKETS) + 1),
            "hist_bytes": [0] * (len(BUCKETS) + 1),
            "bytes_by_nodebucket": [0] * (len(BUCKETS) + 1),
            "depth": {}, "loops": 0, "loop_nodes": 0, "loop_bytes": 0,
            "kinds": {k: 0 for k, _ in KINDS}, "top": []}


def new_acc():
    return {"fn_n": 0, "fn_faithful": 0, "body_bytes": 0, "replay_bytes": 0,
            "attr_bytes": 0,
            "nodes": 0, "ovf": 0, "src_n": 0, "src_fail": 0, "fail_names": [],
            "inv_ind": 0, "inv_ext": 0, "inv_ret": 0, "inv_graft": 0,
            "fn_callfree": 0, "fn_callfree_bytes": 0,
            "part": [new_part(), new_part()]}


def kv(fields):
    d = {}
    for f in fields:
        if "=" in f:
            k, v = f.split("=", 1)
            d[k] = v
    return d


def verify_fn(m, seen):
    """Invariants a per-body record must satisfy, whatever the corpus.

    Transparency can only grow coverage, never shrink it -- it may also raise
    the slice *count*, because a body that was wholly call-covered at t=0 can
    acquire its first slice at t=1, so only nodes and bytes are monotone.  A
    body with no invoke at all has to come out as exactly one slice covering
    every statement and every attributed byte.

    A peephole that rewinds `ind` can retract bytes an earlier statement
    already emitted, so per-statement attribution never loses a byte but can
    double-count a retracted one.  Measured drift on src/mcc.c is 0.005% of
    body bytes at -O1..-O3 and 0.05% at -O0, in four bodies; hence the slack.

    The denominator for a slice extent is `rbytes`, the length the *replay*
    emitted, not `bytes`, the length the parser emitted.  They are the same
    number for a faithful body -- equal length is a precondition of equal
    content -- and that identity is checked here rather than assumed.  An
    unfaithful body is under no obligation to emit the parser's byte count, and
    two on src/mcc.c do not: cst_alloc_node replays 616 B against a 560 B
    parser body and rir_low_set 345 against 307, each because the replay picked
    a different frame layout (loc -16 vs -20, -32 vs -48).  Charging that
    difference to the slice attribution read as a 6%-12% overrun of a body that
    is in fact attributed to the byte.
    """
    out = []
    fn = m["fn"]
    nodes, byts, attr = int(m["nodes"]), int(m["bytes"]), int(m["attr"])
    rbyts = int(m.get("rbytes", byts))
    if int(m["faithful"]) and rbyts != byts:
        out.append("%s: faithful body replayed %d bytes against a %d-byte "
                   "parser body" % (fn, rbyts, byts))
    ns0, nb0, nn0 = int(m["ns0"]), int(m["nb0"]), int(m["nn0"])
    ns1, nb1, nn1 = int(m["ns1"]), int(m["nb1"]), int(m["nn1"])
    ninv = sum(int(m[k]) for k in
               ("inv_ind", "inv_ext", "inv_ret", "inv_graft"))
    if nn0 > nodes or nn1 > nodes:
        out.append("%s: slice nodes %d/%d exceed arena nodes %d"
                   % (fn, nn0, nn1, nodes))
    slack = max(32, rbyts // 50)
    if nb0 > rbyts + slack or nb1 > rbyts + slack:
        out.append("%s: slice bytes %d/%d exceed replayed body bytes %d "
                   "(parser %d) by more than %d"
                   % (fn, nb0, nb1, rbyts, byts, slack))
    if attr > rbyts + slack:
        out.append("%s: statement attribution %d exceeds replayed body bytes "
                   "%d (parser %d) by more than %d"
                   % (fn, attr, rbyts, byts, slack))
    if nn1 < nn0 or nb1 < nb0:
        out.append("%s: t=1 covers less than t=0 (%d/%d/%d vs %d/%d/%d)"
                   % (fn, ns1, nn1, nb1, ns0, nn0, nb0))
    if ninv == 0 and int(m["ovf"]) == 0 and int(m["stmts"]) > 0:
        if ns0 != 1:
            out.append("%s: call-free body split into %d slices" % (fn, ns0))
        if nn0 != nodes - 1:
            out.append("%s: call-free body covers %d of %d-1 nodes"
                       % (fn, nn0, nodes))
        if nb0 != attr:
            out.append("%s: call-free body covers %d of %d attributed bytes"
                       % (fn, nb0, attr))
    if int(m["inv_graft"]) == 0 and (ns1, nn1, nb1) != (ns0, nn0, nb0):
        out.append("%s: no graftable invoke but t=1 differs from t=0" % fn)
    seen[fn] = (ns0, nn0, nb0)
    return out


def absorb(acc, text, top_n, verify=None, seen=None):
    for line in text.splitlines():
        f = line.split()
        if not f:
            continue
        if f[0] == "[slice]":
            m = kv(f[1:])
            t = int(m["t"])
            p = acc["part"][t]
            nodes, byts = int(m["nodes"]), int(m["bytes"])
            kinds = int(m["kinds"], 16)
            p["n"] += 1
            p["nodes"] += nodes
            p["stmts"] += int(m["stmts"])
            if byts >= 0:
                p["bytes"] += byts
            bn = bucket(nodes)
            p["hist_nodes"][bn] += 1
            p["hist_bytes"][bucket(max(byts, 0))] += 1
            p["bytes_by_nodebucket"][bn] += max(byts, 0)
            d = int(m["depth"])
            p["depth"][d] = p["depth"].get(d, 0) + 1
            if int(m["loops"]):
                p["loops"] += 1
                p["loop_nodes"] += nodes
                p["loop_bytes"] += max(byts, 0)
            for name, bit in KINDS:
                if kinds & (1 << bit):
                    p["kinds"][name] += 1
            if top_n:
                p["top"].append((nodes, max(byts, 0), int(m["loops"]), d,
                                 m["fn"]))
                if len(p["top"]) > 4 * top_n:
                    p["top"].sort(reverse=True)
                    del p["top"][top_n:]
        elif f[0] == "[slice-fn]":
            m = kv(f[1:])
            acc["fn_n"] += 1
            acc["fn_faithful"] += int(m["faithful"])
            acc["body_bytes"] += int(m["bytes"])
            acc["replay_bytes"] += int(m.get("rbytes", m["bytes"]))
            acc["attr_bytes"] += int(m["attr"])
            acc["nodes"] += int(m["nodes"])
            acc["ovf"] += int(m["ovf"])
            for k in ("inv_ind", "inv_ext", "inv_ret", "inv_graft"):
                acc[k] += int(m[k])
            if int(m["ns0"]) == 1 and int(m["nb0"]) == int(m["bytes"]):
                acc["fn_callfree"] += 1
                acc["fn_callfree_bytes"] += int(m["bytes"])
            if verify is not None:
                verify.extend(verify_fn(m, seen))


def run_one(mcc, flags, item, opt, tmpdir, idx):
    src, extra = item
    cen = os.path.join(tmpdir, "c%d.txt" % idx)
    out_o = os.path.join(tmpdir, "o%d.o" % idx)
    env = dict(os.environ)
    for k in ("MCC_TEST_OPT", "MCC_RIR_PROD", "MCC_RIR_PROD_OUT",
              "MCC_REPLAY_IR"):
        env.pop(k, None)
    env["MCC_SLICE_CENSUS"] = cen
    if opt == "O0":
        env["MCC_FORCE_REPLAY"] = "1"
    else:
        env.pop("MCC_FORCE_REPLAY", None)
    cmd = [mcc] + flags + extra + ["-" + opt, "-c", src, "-o", out_o]
    p = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT, env=env)
    txt = ""
    if os.path.exists(cen):
        txt = open(cen, errors="replace").read()
        os.unlink(cen)
    if os.path.exists(out_o):
        os.unlink(out_o)
    return p.returncode, txt, (p.stderr or p.stdout or "").strip()


def census(mcc, flags, sources, opt, top_n, jobs, verify=None):
    acc = new_acc()
    with tempfile.TemporaryDirectory() as td:
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            futs = [ex.submit(run_one, mcc, flags, s, opt, td, i)
                    for i, s in enumerate(sources)]
            for item, fu in zip(sources, futs):
                rc, txt, err = fu.result()
                acc["src_n"] += 1
                if rc != 0:
                    acc["src_fail"] += 1
                    acc["fail_names"].append(
                        (os.path.relpath(item[0], ROOT),
                         err.splitlines()[0] if err else "rc=%d" % rc))
                absorb(acc, txt, top_n, verify, {})
    for p in acc["part"]:
        p["top"].sort(reverse=True)
        del p["top"][top_n:]
    return acc


def pct(a, b):
    return 100.0 * a / b if b else 0.0


def corpus_id(corpus, explicit):
    if explicit:
        return "explicit --sources"
    if corpus == "self":
        return "self = src/mcc.c"
    if corpus == "exec":
        return "exec = tests/exec/**.c"
    return ("wide[slice-census] = src/mcc.c + tests/{exec,behavior,ast} + "
            "examples, filtered by tests/exec/goldens.h")


def report(level, acc, top_n, corpus):
    names = bucket_names()
    print("=== -%s  corpus=%s  sources=%d (failed %d)  modelled bodies=%d "
          "(faithful %d)" % (level, corpus, acc["src_n"], acc["src_fail"],
                             acc["fn_n"], acc["fn_faithful"]))
    print("    body bytes %d (replayed %d, %+.3f%%)   statement-attributed %d "
          "(%.3f%% of replayed)   arena nodes %d"
          % (acc["body_bytes"], acc["replay_bytes"],
             pct(acc["replay_bytes"], acc["body_bytes"]) - 100.0,
             acc["attr_bytes"], pct(acc["attr_bytes"], acc["replay_bytes"]),
             acc["nodes"]))
    inv_tot = sum(acc[k] for k in ("inv_ind", "inv_ext", "inv_ret", "inv_graft"))
    print("    invokes %d = indirect %d + opaque-direct %d + retained %d + "
          "graftable %d  (anonymous %d = %.1f%%)"
          % (inv_tot, acc["inv_ind"], acc["inv_ext"], acc["inv_ret"],
             acc["inv_graft"], acc["inv_ind"] + acc["inv_ext"],
             pct(acc["inv_ind"] + acc["inv_ext"], inv_tot)))
    print("    wholly call-free bodies %d/%d (%.1f%%) = %d B (%.2f%% of body "
          "bytes)" % (acc["fn_callfree"], acc["fn_n"],
                      pct(acc["fn_callfree"], acc["fn_n"]),
                      acc["fn_callfree_bytes"],
                      pct(acc["fn_callfree_bytes"], acc["body_bytes"])))
    for t in (0, 1):
        p = acc["part"][t]
        tag = "t=0 every invoke opaque " if t == 0 else "t=1 graftable inlined"
        print("  -- %s : %d slices, %d nodes (%.2f%% of arena), %d B "
              "(%.2f%% of body bytes)"
              % (tag, p["n"], p["nodes"], pct(p["nodes"], acc["nodes"]),
                 p["bytes"], pct(p["bytes"], acc["body_bytes"])))
        print("     nodes/slice histogram (count | bytes in bucket):")
        for i, nm in enumerate(names):
            if not p["hist_nodes"][i]:
                continue
            print("       %-9s %7d  %5.1f%%   %9d B  %5.1f%%"
                  % (nm, p["hist_nodes"][i], pct(p["hist_nodes"][i], p["n"]),
                     p["bytes_by_nodebucket"][i],
                     pct(p["bytes_by_nodebucket"][i], p["bytes"])))
        print("     bytes/slice histogram:")
        for i, nm in enumerate(names):
            if p["hist_bytes"][i]:
                print("       %-9s %7d  %5.1f%%"
                      % (nm, p["hist_bytes"][i], pct(p["hist_bytes"][i], p["n"])))
        print("     loop-nest depth of slice:  " + "  ".join(
            "d%d=%d" % (d, p["depth"][d]) for d in sorted(p["depth"])))
        print("     slices containing a loop: %d (%.2f%%), %d nodes, %d B "
              "(%.2f%% of body bytes)"
              % (p["loops"], pct(p["loops"], p["n"]), p["loop_nodes"],
                 p["loop_bytes"], pct(p["loop_bytes"], acc["body_bytes"])))
        ks = sorted(p["kinds"].items(), key=lambda x: -x[1])
        print("     kinds present in slices: " + "  ".join(
            "%s=%.0f%%" % (k, pct(v, p["n"])) for k, v in ks if v))
        if top_n and p["top"]:
            print("     largest %d slices (nodes, bytes, loops, depth, fn):"
                  % min(top_n, len(p["top"])))
            for nodes, byts, loops, d, fn in p["top"][:top_n]:
                print("       %6d %6d %3d %3d  %s" % (nodes, byts, loops, d, fn))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir")
    ap.add_argument("--levels", default="O0,O1,O2,O3")
    ap.add_argument("--corpus", default="self",
                    choices=["self", "exec", "wide"])
    ap.add_argument("--sources", nargs="*", default=None)
    ap.add_argument("--top", type=int, default=10)
    ap.add_argument("--json", default=None)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--opt-in", action="store_true")
    ap.add_argument("--min-slices", type=int, default=0)
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    if a.opt_in and not os.environ.get("MCC_SLICE_CENSUS_RUN"):
        print("slice-census: set MCC_SLICE_CENSUS_RUN=1 to run this census "
              "(ctest -L census)")
        return 77

    bdir = a.build_dir if os.path.isabs(a.build_dir) \
        else os.path.join(ROOT, a.build_dir)
    mcc = find_mcc(bdir)
    flags = self_flags(bdir)

    walked = a.sources is None
    if a.sources:
        paths = list(a.sources)
    elif a.corpus == "self":
        paths = [os.path.join(ROOT, "src", "mcc.c")]
    else:
        paths = [] if a.corpus == "exec" else [os.path.join(ROOT, "src", "mcc.c")]
        for d in (("tests/exec",) if a.corpus == "exec" else
                  ("tests/exec", "tests/behavior", "tests/ast", "examples")):
            p = os.path.join(ROOT, d)
            if not os.path.isdir(p):
                continue
            for dp, _, fns in os.walk(p):
                for fn in sorted(fns):
                    if fn.endswith(".c"):
                        paths.append(os.path.join(dp, fn))

    cpu, tos = target_id(mcc)
    corpus = corpus_id(a.corpus, not walked)
    sources, dropped = annotate(paths, read_goldens(), cpu, tos, walked)
    if dropped and not a.quiet:
        print("corpus: %d of %d walked sources excluded by tests/exec/goldens.h "
              "on %s/%s; the other %d must all compile"
              % (len(dropped), len(paths), cpu, tos, len(sources)))
        for name, why in dropped:
            print("    %-52s %s" % (name, why))

    out = {}
    bad = []
    for level in a.levels.split(","):
        level = level.strip()
        if not level:
            continue
        errs = [] if a.verify else None
        acc = census(mcc, flags, sources, level, a.top, a.jobs, errs)
        if not a.quiet:
            report(level, acc, a.top, corpus)
        if errs:
            bad += ["-%s %s" % (level, e) for e in errs]
        out[level] = acc

    if a.json:
        json.dump(out, open(a.json, "w"), indent=1, sort_keys=True)

    for level, acc in out.items():
        if acc["src_fail"]:
            bad.append("-%s: %d of %d sources did not compile, so every share "
                       "below is over the survivors. The count reaches stdout "
                       "only via report(), which --quiet suppresses, and "
                       "nothing gated it: 11 of 12 sources could drop out and "
                       "the cell would still pass on the twelfth: %s"
                       % (level, acc["src_fail"], acc["src_n"],
                          "; ".join("%s (%s)" % nm
                                    for nm in acc["fail_names"][:12])))
        if not acc["fn_n"]:
            bad.append("-%s: zero bodies were modelled, so every percentage is "
                       "over an empty denominator and the OK line below would "
                       "read `0 slices (0.0%% of 0 body bytes)` as a result"
                       % level)
        if a.min_slices and acc["part"][0]["n"] < a.min_slices:
            bad.append("-%s: enumerated %d slices, expected >= %d"
                       % (level, acc["part"][0]["n"], a.min_slices))
    if bad:
        for b in bad[:40]:
            print("slice-census: " + b)
        if len(bad) > 40:
            print("slice-census: ... and %d more" % (len(bad) - 40))
        return 1
    if a.quiet:
        for level, acc in out.items():
            p = acc["part"][0]
            print("slice-census -%s: corpus=%s, %d sources, %d bodies, %d "
                  "slices, %d B in slices (%.1f%% of %d body bytes)  OK"
                  % (level, corpus, acc["src_n"], acc["fn_n"], p["n"],
                     p["bytes"], pct(p["bytes"], acc["body_bytes"]),
                     acc["body_bytes"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
