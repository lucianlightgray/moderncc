#!/usr/bin/env python3
"""Every citation in the live docs resolves against the tree it cites.

The recurring failure this closes is a doc that names a symbol, a file or a
line that used to exist.  The retired `docs/PLAN.md` named `MCC_MAX_UNARY_DEPTH` at
`src/mccgen.c:241` for a full round after `wt/unarydepth` replaced it with
`MCC_MAX_PARSE_DEPTH`; `README.md` advertised four `-DMCC_CONFIG_*` build
options the build had not had since `a55c0a07`, so passing one left an ignored
cache entry and nothing said so.  Nothing checked either, because nothing
checked anything: no tool in this tree opened a file under `docs/` at all.

Four rules, chosen so the check is useful rather than noisy.  Prose names are
deliberately out of scope -- a design doc is allowed, and required, to name
things that do not exist yet, and every rule below is written so that a
proposal cannot trip it.

  path    a backticked path rooted in this tree (`src/`, `tools/`, `tests/`,
          `cmake/`, `include/`, `runtime/`, `examples/`, `docs/`, `.github/`,
          or a top-level file) and carrying a source-ish extension must exist.
  line    a `path:N` or `path:N-M` citation must land inside that file.  This
          catches a file that shrank past its anchor; it cannot catch an
          anchor that drifted onto different content, and does not pretend to.
  symbol  a backticked span whose first token is namespaced to this project
          (`mcc_`, `ast_`, `rir_`, `spv_`, `msl_`, `jrn_`, `MCC_`, `AST_`,
          `RIR_`, `SPV_`) and which is immediately followed by a backticked
          in-tree path *carrying a line anchor* must occur in that file, as a
          whole token or as the prefix of one.  Both halves of that are load
          bearing.  The adjacency is what separates a citation from a mention:
          "`NAME` (`src/f.c:123`)" is a claim about a location, a bare
          "`NAME`" in a sentence is not.  The line anchor is what separates a
          claim from its negation: this file is full of true sentences of the
          form "`NAME` has zero hits in `src/f.c`", and they do not carry one.

A path whose directory does not exist in this tree is not a claim about this
tree and is skipped -- that is how `src/gallium/frontends/lavapipe/lvp_device.c`
stays quotable.  The cost is that a file deleted along with its whole directory
escapes the path rule; it is priced and accepted.
  count   the failed-to-reproduce table in docs/TODO.md is counted, and the
          one sentence that states its size must agree.  Three sentences used
          to state it as twelve, nine and seven over a thirteen-row table.

`docs/ARCHIVED.md` is out of scope on purpose and by its own header: it is a
snapshot kept "for later validation", and most of what it cites was deleted
after the snapshot was taken.  Demanding that its anchors resolve would demand
that history be rewritten.  `--include-archived` reports on it without gating.

`tools/docref-allow.txt` holds the citations that must not resolve -- a design
doc naming a file it proposes to write, a paragraph whose subject is that
something was deleted.  It is ratcheted from both ends: an entry that starts
resolving fails, and so does an entry no live doc cites any more.  A list of
exceptions nobody needs is how the next real one gets waved through.

Exit 0 clean, 1 on any violation, 2 on a usage or parse problem.  Never 77: a
citation that cannot be checked is a failure, not a skip -- an unreadable doc
tree is exactly the state this cell exists to refuse.
"""

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DOCS = ["docs/TODO.md", "README.md"]
ARCHIVED = "docs/ARCHIVED.md"

ROOTED = ("src/", "tools/", "tests/", "cmake/", "include/", "runtime/",
          "examples/", "docs/", ".github/")
TOPFILE = ("CMakeLists.txt", "CMakePresets.json", "README.md")
SRC_EXT = (".c", ".h", ".py", ".sh", ".cmake", ".txt", ".json", ".tsv", ".yml",
           ".yaml", ".md", ".S", ".m", ".inc", ".cc", ".cpp")
SKIP_DIR = (".git", "vendor", "node_modules")

TICK = re.compile(r"`([^`\n]+)`")
PATHREF = re.compile(r"^([A-Za-z0-9_./+-]+\.[A-Za-z0-9]+)"
                     r"(?::(\d+)(?:-(\d+))?)?$")
NS = re.compile(r"^(mcc|ast|rir|spv|msl|jrn|MCC|AST|RIR|SPV)_[A-Za-z0-9_]+$")
SITE = re.compile(r"`([A-Za-z_][A-Za-z0-9_]*)[^`\n]*`[^`\n]{0,48}?"
                  r"`([A-Za-z0-9_./+-]+\.[A-Za-z0-9]+):\d+(?:[-,:]\d+)*`")

TABLE_HEAD = "| figure | actual | how it failed |"
TABLE_COUNT = re.compile(r"\*\*The (\d+) figures that have failed to reproduce\*\*")

ALLOW = "tools/docref-allow.txt"


def rooted(path):
    return path.startswith(ROOTED) or path in TOPFILE


def wanted(path):
    return rooted(path) and path.endswith(SRC_EXT)


def walk_tree(root):
    files, dirs = {}, {""}
    for dp, dns, fns in os.walk(root):
        dns[:] = [d for d in dns
                  if d not in SKIP_DIR and not d.startswith("cmake-")]
        # Key on forward slashes: citations in the docs are POSIX-style, but
        # os.path.relpath yields the native separator, so on Windows the raw keys
        # (docs\TODO.md) never match a cited docs/TODO.md and every ref reads as
        # "not in the tree".
        rel = os.path.relpath(dp, root).replace(os.sep, "/")
        dirs.add("" if rel == "." else rel)
        for fn in fns:
            files[os.path.relpath(os.path.join(dp, fn),
                                  root).replace(os.sep, "/")] = None
    return files, dirs


def body(root, rel, cache):
    if rel not in cache:
        try:
            cache[rel] = open(os.path.join(root, rel), encoding="utf-8",
                              errors="replace").read()
        except OSError:
            cache[rel] = ""
    return cache[rel]


def nlines(root, rel, cache):
    return body(root, rel, cache).count("\n") + 1


def read_allow(root):
    out = {}
    path = os.path.join(root, ALLOW)
    if not os.path.exists(path):
        return out
    for i, line in enumerate(open(path, encoding="utf-8"), 1):
        line = line.split("#", 1)[0].strip()
        if line:
            out[line] = i
    return out


def occurs(text, ident):
    return re.search(r"\b" + re.escape(ident) + r"[A-Za-z0-9_]*", text) is not None


def check_doc(root, doc, files, dirs, cache, allow, cited, mutate):
    lines = open(os.path.join(root, doc), encoding="utf-8").read().split("\n")
    if mutate:
        lines = list(lines)
        lines.append("planted: `src/mccgen.c:999999` is past the end of that file")
        lines.append("planted: `src/mcc_no_such_file.c` names nothing")
        lines.append("planted: `MCC_MAX_UNARY_DEPTH 2048` (`src/mccgen.c:241`)")
    seen = {"path": 0, "line": 0, "site": 0}
    bad = []
    for n, line in enumerate(lines, 1):
        for tok in TICK.findall(line):
            m = PATHREF.match(tok.strip())
            if not m or not wanted(m.group(1)):
                continue
            path, l1, l2 = m.group(1), m.group(2), m.group(3)
            if os.path.dirname(path) not in dirs:
                continue
            seen["path"] += 1
            cited.add(path)
            if path not in files:
                if path in allow:
                    continue
                bad.append(("path", "%s:%d cites `%s`, which is not in the "
                            "tree. Either the citation is stale or the file "
                            "moved; if it must not resolve, say why in %s"
                            % (doc, n, tok, ALLOW)))
                continue
            cap = nlines(root, path, cache)
            for lit in (l1, l2):
                if lit is None:
                    continue
                seen["line"] += 1
                if int(lit) < 1 or int(lit) > cap:
                    bad.append(("line", "%s:%d cites `%s`, but %s is %d lines "
                                "long. The anchor is past the end of the file "
                                "it names" % (doc, n, tok, path, cap)))
        for m in SITE.finditer(line):
            ident, path = m.group(1), m.group(2)
            if not NS.match(ident) or not wanted(path) or path not in files:
                continue
            seen["site"] += 1
            if not occurs(body(root, path, cache), ident):
                bad.append(("site", "%s:%d cites `%s` at `%s`, and that symbol "
                            "does not occur in that file. A symbol quoted "
                            "beside a location is a claim about the location"
                            % (doc, n, ident, path)))
    return seen, bad


def check_table(root, mutate):
    doc = "docs/TODO.md"
    lines = open(os.path.join(root, doc), encoding="utf-8").read().split("\n")
    head = [i for i, ln in enumerate(lines) if ln.strip() == TABLE_HEAD]
    if len(head) != 1:
        return 0, [("count", "%s: expected exactly one `%s` header line, "
                    "found %d. The failed-to-reproduce table is the only "
                    "statement of its own size and this check cannot find it"
                    % (doc, TABLE_HEAD, len(head)))]
    i = head[0] + 2
    rows = 0
    while i < len(lines) and lines[i].startswith("|"):
        rows += 1
        i += 1
    if mutate:
        rows += 1
    stated = None
    for ln in lines[max(0, head[0] - 12):head[0]]:
        m = TABLE_COUNT.search(ln)
        if m:
            stated = int(m.group(1))
    if stated is None:
        return rows, [("count", "%s: the failed-to-reproduce table has %d rows "
                       "and no sentence above it states that count. State it "
                       "once, in the form the lint reads, or three sentences "
                       "will state it three ways again" % (doc, rows))]
    if stated != rows:
        return rows, [("count", "%s: the failed-to-reproduce table has %d rows "
                       "and the sentence above it says %d. The table is the "
                       "authority" % (doc, rows, stated))]
    return rows, []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=ROOT)
    # Match the floor docs/refs registers. A default ABOVE the real count made
    # the tool fail by hand on a tree whose own gate was green -- it defaulted
    # to 600 while the cell passed 440 and the tree resolved 510, then 584. An
    # anti-vacuity floor that only the gate gets right teaches its readers that
    # the tool is broken rather than that the docs are.
    ap.add_argument("--min-refs", type=int, default=440)
    ap.add_argument("--include-archived", action="store_true")
    ap.add_argument("--mutate", action="store_true",
                    help="plant one defect of every shape this lint claims to "
                         "catch, in memory. The arm it modifies must fail, and "
                         "must fail for all four reasons")
    a = ap.parse_args()

    root = os.path.abspath(a.root)
    missing = [d for d in DOCS if not os.path.exists(os.path.join(root, d))]
    if missing:
        sys.stderr.write("docref-lint: %s does not exist under %s. A doc lint "
                         "that cannot find the docs must not report OK\n"
                         % (", ".join(missing), root))
        return 2

    files, dirs = walk_tree(root)
    cache = {}
    allow = read_allow(root)
    docs = list(DOCS) + ([ARCHIVED] if a.include_archived else [])

    seen = {"path": 0, "line": 0, "site": 0}
    bad = []
    cited = set()
    for doc in docs:
        s, b = check_doc(root, doc, files, dirs, cache, allow, cited,
                         a.mutate and doc == "docs/TODO.md")
        for k in seen:
            seen[k] += s[k]
        bad += b
    rows, b = check_table(root, a.mutate)
    bad += b
    for path in sorted(allow):
        if path in files:
            bad.append(("allow", "%s:%d lists `%s` as a citation that must not "
                        "resolve, and it does resolve. Remove the entry -- an "
                        "allowance that has outlived its reason hides the next "
                        "real one" % (ALLOW, allow[path], path)))
        elif path not in cited and not a.mutate:
            bad.append(("allow", "%s:%d allows `%s` and no live doc cites it. "
                        "Remove the entry; a list of exceptions nobody needs is "
                        "how the next real one gets waved through"
                        % (ALLOW, allow[path], path)))

    total = seen["path"] + seen["site"]
    if total < a.min_refs:
        sys.stderr.write("docref-lint: found %d citation(s) across %d doc "
                         "file(s), expected at least %d. A lint that resolved "
                         "nothing renders identically to a tree with nothing "
                         "wrong in it\n" % (total, len(docs), a.min_refs))
        return 1
    if not rows:
        sys.stderr.write("docref-lint: the failed-to-reproduce table was not "
                         "found or is empty, so its count was not checked\n")
        return 1

    if a.mutate:
        for cls, m in bad:
            print("FAIL " + m)
        got = {c for c, _ in bad}
        want = {"path", "line", "site", "count"}
        blind = want - got
        if blind:
            print("docref-lint --mutate: planted one defect of every shape and "
                  "the lint did not report %s, so it is not reading what it "
                  "claims to read" % ", ".join(sorted(blind)))
            return 0
        print("docref-lint --mutate: %d violation(s), all four planted shapes "
              "reported (%s)" % (len(bad), ", ".join(sorted(want))))
        return 1

    census = ("%d doc file(s), %d in-tree path citation(s) (%d with a line "
              "anchor), %d symbol-at-a-location claim(s), %d allowed "
              "non-resolving, failed-to-reproduce table %d rows"
              % (len(docs), seen["path"], seen["line"], seen["site"],
                 len(allow), rows))
    for cls, m in bad:
        print("docref-lint: " + m)
    if bad:
        print("docref-lint: %d dangling citation(s) over %s" % (len(bad), census))
        return 1
    print("docref-lint: OK -- " + census)
    return 0


if __name__ == "__main__":
    sys.exit(main())
