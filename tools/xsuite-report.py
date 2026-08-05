#!/usr/bin/env python3
"""Summarize a tools/xsuite.py results.jsonl: per-suite tallies, top failure
signatures, and the -O0 -> -O3 delta (tests that only fail with the optimizer on).

Usage: tools/xsuite-report.py <out-dir> [--top N]
"""
import collections, json, os, sys

def main():
    out = sys.argv[1]
    top = int(sys.argv[sys.argv.index("--top") + 1]) if "--top" in sys.argv else 15
    rows = [json.loads(l) for l in open(os.path.join(out, "results.jsonl"))]

    skips = collections.Counter(r["suite"] for r in rows
                                if r["status"] in ("SKIP", "REFSKIP"))
    tally = collections.defaultdict(collections.Counter)
    byfile = collections.defaultdict(dict)
    for r in rows:
        if r["status"] in ("SKIP", "REFSKIP"):
            continue
        tally[(r["suite"], r["opt"])][r["status"]] += 1
        byfile[r["file"]][r["opt"]] = r

    cols = ["PASS", "FAIL", "FAILEXE", "ICE", "TIMEOUT", "XPASS"]
    opts = sorted({o for _, o in tally})
    print(f"{'suite':<24}{'opt':>5}" + "".join(f"{c:>9}" for c in cols) +
          f"{'rate':>8}{'skipped':>9}")
    print("-" * (24 + 5 + 9 * len(cols) + 17))
    tot = collections.defaultdict(collections.Counter)
    for suite in sorted({s for s, _ in tally}):
        for o in opts:
            t = tally[(suite, o)]
            n = sum(t.values())
            rate = 100.0 * t["PASS"] / n if n else 0.0
            print(f"{suite:<24}{o:>5}" + "".join(f"{t[c]:>9}" for c in cols) +
                  f"{rate:>7.1f}%" + f"{skips[suite] if o == opts[0] else '':>9}")
            tot[o].update(t)
    print("-" * (24 + 5 + 9 * len(cols) + 17))
    for o in opts:
        n = sum(tot[o].values())
        print(f"{'TOTAL':<24}{o:>5}" + "".join(f"{tot[o][c]:>9}" for c in cols) +
              f"{100.0 * tot[o]['PASS'] / n if n else 0:>7.1f}%" +
              f"{sum(skips.values()) if o == opts[0] else '':>9}")

    if len(opts) > 1:
        a, b = opts[0], opts[-1]
        reg = [f for f, d in byfile.items()
               if a in d and b in d and d[a]["status"] == "PASS" and d[b]["status"] != "PASS"]
        fix = [f for f, d in byfile.items()
               if a in d and b in d and d[a]["status"] != "PASS" and d[b]["status"] == "PASS"]
        print(f"\n{b}-only failures (pass at {a}): {len(reg)}")
        for f in sorted(reg)[:top]:
            d = byfile[f][b]
            print(f"  {d['status']:<8} {f}  {d.get('err', '')[:70]}")
        if len(reg) > top:
            print(f"  ... {len(reg) - top} more (see {out}/o3-only-failures.txt)")
        with open(os.path.join(out, "o3-only-failures.txt"), "w") as fh:
            for f in sorted(reg):
                d = byfile[f][b]
                fh.write(f"{d['status']}\t{f}\t{d.get('err', '')}\n")
        print(f"{a}-only failures (pass at {b}): {len(fix)}")

    buckets = [
        ("gnu-builtin", r"__builtin|__sync_|__atomic_"),
        ("vector-ext", r"vector_size|vector type"),
        ("nested-function", r"local functions"),
        ("complex", r"_Complex|__real|__imag"),
        ("int128/bitint", r"__int128|_BitInt"),
        ("inline-asm", r"unknown opcode|unknown constraint|asm operand|clobber"),
        ("attribute", r"attribute"),
        ("missing-header", r"include file"),
        ("c23/embed", r"#embed|_Generic|typeof_unqual|constexpr"),
        ("implicit-decl", r"implicit declaration|implicit int"),
        ("parse", r"expected|undeclared|declaration|identifier"),
    ]
    import re as _re
    print("\nfailure buckets (first match wins):")
    fails = [r for r in rows if r["status"] in ("FAIL", "ICE")]
    bc = collections.Counter()
    for r in fails:
        err = r.get("err", "")
        bc[next((n for n, p in buckets if _re.search(p, err)), "other")] += 1
    for n, v in bc.most_common():
        print(f"  {v:>6}  {n}")

    print("\ntop failure signatures:")
    sig = collections.Counter(r.get("err", "") for r in rows
                              if r["status"] in ("FAIL", "ICE"))
    for k, v in sig.most_common(top):
        print(f"  {v:>6}  {k}")

    print("\nnon-compile failures (wrong answer at runtime / hangs):")
    for st in ("FAILEXE", "TIMEOUT", "ICE", "XPASS"):
        fs = sorted({r["file"] for r in rows if r["status"] == st})
        print(f"  {st}: {len(fs)}")
        with open(os.path.join(out, f"{st.lower()}.txt"), "w") as fh:
            for f in fs:
                fh.write(f + "\n")


if __name__ == "__main__":
    main()
