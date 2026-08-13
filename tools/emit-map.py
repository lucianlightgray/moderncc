#!/usr/bin/env python3
"""Emit map: what each layer of mcc does for every function body it compiles.

Runs a trace-enabled mcc (MCC_CONFIG_TRACE=ON) over a target, segments the
MCC_TRACE stream by compiled body, and reconciles the machine-code emit calls
against the bytes that actually survive into the object.

The four layers are AOT codegen (mccgen.c + arch), RIR (mccrir.c), AST
(mccast.c) and JRN/ir_cap (mccircap.c).  MCC_INV=1 supplies the authoritative
per-layer totals in the same run on the [invcount] channel, so the
trace-derived numbers can be checked against a counter that was validated as
non-perturbing.  That check is check_inv_against_anchors() and it is enforced:
until 2026-08-13 the docstring claimed it and no code did it, while two of the
five anchors were optional, so an anchor that stopped resolving reported both
dropout counters as 0 and nothing failed.
"""

import argparse
import json
import os
import re
import subprocess
import sys


def die(msg):
    sys.stderr.write("emit-map: %s\n" % msg)
    raise SystemExit(2)


def find_anchors(srcdir):
    """Resolve the handful of file:line anchors we key on, by symbol.

    docs/TODO.md warns that hardcoded line anchors in this tree drift by
    hundreds of lines across merges, so nothing here is hardcoded.
    """
    a = {}
    gen = os.path.join(srcdir, "mccgen.c")
    ast = os.path.join(srcdir, "mccast.c")
    with open(gen, encoding="utf8", errors="replace") as f:
        lines = f.readlines()
    for i, ln in enumerate(lines):
        if re.match(r"\s*ST_FUNC void g\(int c\)", ln):
            a["g_enter"] = i + 1
            for j in range(i, min(i + 12, len(lines))):
                if "nocode_wanted" in lines[j]:
                    a["g_nocode"] = j + 2
                    break
        if re.match(r"\s*static void gen_function\(Sym \*sym\)", ln):
            for j in range(i, min(i + 4, len(lines))):
                if "MCC_TRACE" in lines[j] and "get_tok_str" in lines[j]:
                    a["body"] = j + 1
                    break
    with open(ast, encoding="utf8", errors="replace") as f:
        lines = f.readlines()
    for i, ln in enumerate(lines):
        if 'rir_prod_why_set("replayok")' in ln:
            for j in range(i - 1, max(i - 4, 0), -1):
                if "else" in lines[j] and "MCC_TRACE" in lines[j]:
                    a["ast_replayok"] = j + 1
                    break
        if 'ast_unf_why = "posterr"' in ln:
            for j in range(i, max(i - 60, 0), -1):
                if re.match(r"\s*\} else \{", lines[j]) and "MCC_TRACE" in lines[j]:
                    a["ast_abort"] = j + 1
                    break
    missing = [k for k in ("g_enter", "g_nocode", "body", "ast_abort",
                           "ast_replayok") if k not in a]
    if missing:
        die("could not resolve anchors by symbol: %s.\n"
            "  ast_abort and ast_replayok were optional until 2026-08-13, and "
            "that is how this tool\n"
            "  reported both dropout counters as 0 and inflated gap_unexplained "
            "by exactly the\n"
            "  aborts it had stopped seeing.  An anchor that stops resolving is "
            "a source move, not\n"
            "  a measurement: re-point it rather than making it optional again."
            % ", ".join(missing))
    return a


def run_capture(cmd):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL, timeout=60)
        return p.returncode, p.stdout.decode("utf8", "replace")
    except (OSError, subprocess.TimeoutExpired):
        return 1, ""


def target_arch(mcc):
    rc, out = run_capture([mcc, "-dM", "-E", "-x", "c", os.devnull])
    if rc != 0:
        return None
    for tok, name in (("__x86_64__", "x86_64"), ("__i386__", "i386"),
                      ("__aarch64__", "arm64"), ("__arm__", "arm"),
                      ("__riscv", "riscv64")):
        if ("#define %s " % tok) in out:
            return name
    return None


def build_cmd(mcc, root, bdir, target, opt, embed_jit, extra):
    if target == "selfhost":
        cc = os.path.join(bdir, "compile_commands.json")
        if not os.path.exists(cc):
            die("no compile_commands.json in %s" % bdir)
        with open(cc, encoding="utf8") as f:
            db = json.load(f)
        rec = [x for x in db if x["file"].endswith("/mcc.c")]
        if not rec:
            die("no src/mcc.c record in compile_commands.json")
        import shlex
        flags = [x for x in shlex.split(rec[0]["command"])[1:]
                 if (x.startswith("-D") or x.startswith("-I"))
                 and not x.endswith(".c")
                 and not x.startswith("-DMCC_CONFIG_TRACE")]
        src = os.path.join(root, "src", "mcc.c")
    else:
        flags = ["-I" + os.path.join(root, "runtime", "include"), "-I" + root,
                 "-I" + bdir, "-DCC_NAME=CC_gcc", "-DGCC_MAJOR=15"]
        src = os.path.join(root, "tests", "diff", target)
    cmd = [mcc, "-B" + bdir] + flags + ["-w", opt]
    if embed_jit:
        cmd.append("--embed-jit")
    cmd += list(extra) + ["-c", src, "-o", os.devnull]
    return cmd


def run(cmd, anchors, root, per_body):
    """Stream the trace through grep, then segment it by compiled body."""
    gen = os.path.join(root, "src", "mccgen.c")
    ast = os.path.join(root, "src", "mccast.c")
    astpat = "|".join(str(anchors[k]) for k in ("ast_abort", "ast_replayok"))
    pat = (r"mccgen\.c:(%d|%d|%d) |mccast\.c:(%s) |ast_replay_body: enter|"
           r"/mccrir\.c:|/mccircap\.c:|^\[invcount\]"
           % (anchors["g_enter"], anchors["g_nocode"], anchors["body"], astpat))
    env = dict(os.environ, MCC_LOG="128", MCC_INV="1")
    p1 = subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                          stderr=subprocess.PIPE, env=env)
    p2 = subprocess.Popen(["grep", "-E", pat], stdin=p1.stderr,
                          stdout=subprocess.PIPE)
    p1.stderr.close()

    body_tag = "%s:%d " % (gen, anchors["body"])
    g_enter_tag = "%s:%d " % (gen, anchors["g_enter"])
    g_nocode_tag = "%s:%d " % (gen, anchors["g_nocode"])

    abort_tag = "%s:%d " % (ast, anchors["ast_abort"])
    replayok_tag = "%s:%d " % (ast, anchors["ast_replayok"])

    bodies, cur, inv = [], None, {}
    tot = dict(g_enter=0, g_nocode=0, replay=0, rir=0, ircap=0, bodies=0,
               ast_abort=0, ast_replayok=0, take=0)

    def flush():
        if cur is not None:
            bodies.append(cur)

    for raw in p2.stdout:
        ln = raw.decode("utf8", "replace")
        if ln.startswith("[invcount]"):
            for kv in ln.split()[1:]:
                k, eq, v = kv.partition("=")
                if not eq or not re.fullmatch(r"-?\d+", v):
                    die("unparsable [invcount] token %r.  The counter channel is "
                        "key=integer only; a\n  reason NAME on that line needs a "
                        "channel of its own." % kv)
                inv[k] = int(v)
            continue
        if body_tag in ln:
            flush()
            cur = dict(name=ln.rsplit(": ", 1)[-1].strip(), g_enter=0,
                       g_nocode=0, replay=0, rir=0, ircap=0,
                       ast_abort=0, ast_replayok=0, take=0)
            tot["bodies"] += 1
            continue
        if g_enter_tag in ln:
            key = "g_enter"
        elif g_nocode_tag in ln:
            key = "g_nocode"
        elif abort_tag and abort_tag in ln:
            key = "ast_abort"
        elif replayok_tag and replayok_tag in ln:
            key = "ast_replayok"
        elif "ast_replay_body: enter" in ln:
            key = "replay"
        elif "/mccrir.c:" in ln:
            key = "rir"
        elif "/mccircap.c:" in ln:
            key = "ircap"
        else:
            continue
        tot[key] += 1
        if cur is not None:
            cur[key] += 1
        if key == "rir" and "rir_prod_take: enter" in ln:
            tot["take"] += 1
            if cur is not None:
                cur["take"] += 1
    flush()
    p2.stdout.close()
    p2.wait()
    rc = p1.wait()
    if rc != 0:
        die("compile failed (rc=%d)" % rc)
    if per_body:
        with open(per_body, "w", encoding="utf8") as f:
            f.write("name\tg_enter\tg_nocode\tg_write\treplay\trir\tircap\n")
            for b in bodies:
                f.write("%s\t%d\t%d\t%d\t%d\t%d\t%d\n"
                        % (b["name"], b["g_enter"], b["g_nocode"],
                           b["g_enter"] - b["g_nocode"], b["replay"],
                           b["rir"], b["ircap"]))
    return tot, inv, bodies


def summarise(tot, inv, bodies):
    g_write = tot["g_enter"] - tot["g_nocode"]
    aot_bytes = inv.get("aot.bytes", 0)
    rir_body = inv.get("rir.body", 0)
    verdict = inv.get("ast.body", 0)
    s = {
        "bodies_traced": tot["bodies"],
        "g_enter": tot["g_enter"],
        "g_nocode_dropped": tot["g_nocode"],
        "g_bytes_written": g_write,
        "aot_bytes_surviving": aot_bytes,
        "emit_amplification": round(g_write / aot_bytes, 4) if aot_bytes else None,
        "replay_passes": tot["replay"],
        "replay_per_verdict": round(tot["replay"] / verdict, 4) if verdict else None,
        "rir_events": tot["rir"],
        "ircap_events": tot["ircap"],
        "multi_replay_bodies": sum(1 for b in bodies if b["replay"] > 1),
        "zero_emit_bodies": sum(1 for b in bodies
                                if b["g_enter"] - b["g_nocode"] == 0),
        "dropout_abort": tot["ast_abort"],
        "dropout_replayok": tot["ast_replayok"],
        "recorded_bodies": sum(1 for b in bodies if b["take"] > 0),
        "dropout_no_replay": sum(1 for b in bodies
                                 if b["take"] > 0 and b["replay"] == 0),
    }
    for k in ("rir.body", "rir.rec", "ast.body", "ast.arena", "ast.faithful",
              "jit.baked", "jit.embed", "jit.embed_bytes", "aot.fn",
              "ast.parser_bytes", "ast.replay_bytes", "ast.abort",
              "ast.abort_post", "ast.noreplay"):
        s[k] = inv.get(k, 0)
    s["inv_abort"] = inv.get("ast.abort", 0) + inv.get("ast.abort_post", 0)
    s["inv_noreplay"] = inv.get("ast.noreplay", 0)
    s["anchor_abort_matches_inv"] = tot["ast_abort"] == s["inv_abort"]
    s["bake_attempts_wasted"] = inv.get("jit.baked", 0) - inv.get("jit.embed", 0)
    s["pct_recorded"] = round(100.0 * inv.get("rir.rec", 0) / rir_body, 2) if rir_body else None
    s["pct_verdicted"] = round(100.0 * verdict / rir_body, 2) if rir_body else None
    s["pct_faithful"] = round(100.0 * inv.get("ast.faithful", 0) / verdict, 2) if verdict else None
    s["gap_recorded_to_verdict"] = inv.get("rir.rec", 0) - verdict
    s["gap_emitted_to_verdict"] = inv.get("aot.fn", 0) - verdict
    s["gap_explained"] = s["dropout_abort"] + s["dropout_no_replay"]
    s["gap_unexplained"] = s["gap_recorded_to_verdict"] - s["gap_explained"]
    return s


def check_inv_against_anchors(s):
    """The cross-check this tool's docstring has always claimed to perform.

    dropout_abort is counted by walking the trace to a line number resolved from
    a source pattern; ast.abort/ast.abort_post are counted by the compiler at
    the site.  They measure the same event, so a disagreement is either an
    anchor that has drifted onto a line that traces something else or a counter
    that is not where it says it is.  Both are findings.  Silence was the bug.
    """
    if s["anchor_abort_matches_inv"]:
        return []
    return ["dropout_abort %d (trace anchor) != ast.abort+ast.abort_post %d "
            "(MCC_INV counters at the site).  gap_explained and "
            "gap_unexplained are derived from the first, so both are wrong by "
            "the difference." % (s["dropout_abort"], s["inv_abort"])]


BANK_KEYS = ("emit_amplification", "replay_per_verdict", "pct_recorded",
             "pct_verdicted", "pct_faithful", "rir_layer_traced",
             "anchor_abort_matches_inv")
TOL = {"emit_amplification": 0.05, "replay_per_verdict": 0.05,
       "pct_recorded": 1.0, "pct_verdicted": 1.0, "pct_faithful": 1.0}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bdir")
    ap.add_argument("--root", default=None)
    ap.add_argument("--target", default="full_language.c")
    ap.add_argument("--opt", default="-O1")
    ap.add_argument("--embed-jit", action="store_true")
    ap.add_argument("--flag", action="append", default=[])
    ap.add_argument("--per-body", default=None)
    ap.add_argument("--bank", default=None)
    ap.add_argument("--update-bank", action="store_true")
    ap.add_argument("--opt-in", action="store_true")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    if args.opt_in and not os.environ.get("MCC_EMIT_MAP"):
        print("SKIP: emit-map census is opt-in (set MCC_EMIT_MAP=1)")
        return 77

    root = args.root or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bdir = args.bdir if os.path.isabs(args.bdir) else os.path.join(root, args.bdir)
    mcc = os.path.join(bdir, "mcc")
    if not os.path.exists(mcc):
        print("SKIP: no mcc in %s" % bdir)
        return 77

    arch = target_arch(mcc)
    if arch not in ("x86_64", "i386"):
        print("SKIP: the byte census needs g() as the single emit primitive, which is "
              "true on x86_64 and i386 only -- on %s each o() writes "
              "cur_text_section->data directly and the arm/arm64 assemblers add a second "
              "cursor writer each, so emit_amplification would be a small nonzero number "
              "measured over the wrong denominator" % (arch or "an unrecognised target"))
        return 77

    srcdir = os.path.join(root, "src")
    anchors = find_anchors(srcdir)

    with open(os.path.join(srcdir, "mccrir.c"), encoding="utf8",
              errors="replace") as f:
        rir_traced = "MCC_TRACE(" in f.read()

    cmd = build_cmd(mcc, root, bdir, args.target, args.opt, args.embed_jit,
                    args.flag)
    tot, inv, bodies = run(cmd, anchors, root, args.per_body)
    if not inv:
        print("SKIP: no [invcount] line - build lacks MCC_INV instrumentation")
        return 77
    if inv.get("inv.dropped"):
        die("the compiler's inventory table overflowed MCC_INV_MAX and dropped "
            "%d increment(s).\n  Every key it lost reads as a genuine zero here."
            % inv["inv.dropped"])
    if tot["bodies"] == 0:
        print("SKIP: no trace output - build lacks MCC_CONFIG_TRACE=ON")
        return 77

    s = summarise(tot, inv, bodies)
    s["rir_layer_traced"] = rir_traced

    if args.json:
        print(json.dumps(s, indent=1, sort_keys=True))
    else:
        key = "%s%s%s" % (args.target, args.opt,
                          " --embed-jit" if args.embed_jit else "")
        print("emit-map %s" % key)
        for k in sorted(s):
            print("  %-26s %s" % (k, s[k]))

    drift = check_inv_against_anchors(s)
    if drift:
        for d in drift:
            print("FAIL: %s" % d)
        return 1

    if args.bank:
        bkey = "%s|%s|%s|%s" % (arch, args.target, args.opt,
                                "jit" if args.embed_jit else "nojit")
        bank = {}
        if os.path.exists(args.bank):
            with open(args.bank, encoding="utf8") as f:
                bank = json.load(f)
        cell = {k: s[k] for k in BANK_KEYS if k in s}
        if args.update_bank:
            bank[bkey] = cell
            with open(args.bank, "w", encoding="utf8") as f:
                json.dump(bank, f, indent=1, sort_keys=True)
                f.write("\n")
            print("banked %s" % bkey)
            return 0
        if bkey not in bank:
            print("SKIP: no banked map for %s (use --update-bank)" % bkey)
            return 77
        bad = []
        for k, want in bank[bkey].items():
            got = cell.get(k)
            if isinstance(want, bool) or isinstance(got, bool):
                if got != want:
                    bad.append("%s: banked %s, now %s" % (k, want, got))
            elif want is None or got is None:
                if got != want:
                    bad.append("%s: banked %s, now %s" % (k, want, got))
            elif abs(got - want) > TOL.get(k, 0.01):
                bad.append("%s: banked %s, now %s (tolerance %s)"
                           % (k, want, got, TOL.get(k, 0.01)))
        if bad:
            for b in bad:
                print("FAIL: %s" % b)
            return 1
        print("emit-map bank OK for %s" % bkey)
    return 0


if __name__ == "__main__":
    sys.exit(main())
