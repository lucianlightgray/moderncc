#!/usr/bin/env python3
"""Run an external C corpus through mcc's slice/GPU path against a CROSS oracle.

The cross oracle is the point. Validating gcc's execute corpus with gcc, or
LLVM's with clang, bakes each suite's own assumptions into the verdict; a
program only becomes evidence here when the OTHER compiler agrees with the
suite's own, at two optimisation levels, so that a UB-dependent program cannot
be mistaken for an mcc failure.

Two things are measured and they are not the same question.

  1. The funnel. How many programs produce a slice at all, how many of those
     lower to a device kernel, how many dispatch, how many agree. The device
     backend is frozen and `mcc_slice_frame_from_ast` has no caller in src/, so
     the honest answer is a count at every stage rather than a pass rate.

  2. The CPU reference against the oracle. `ast_eval_slice` is a second
     implementation of C integer semantics; slicerun --cref writes each accepted
     slice back out as C so gcc and clang can answer the same tuples. This runs
     whether or not a device exists, and a disagreement here is a semantic bug
     in the compiler, not a coverage gap.

Exit 0 clean, 1 on any violation or unmet floor, 2 on a usage problem, 77 when a
stage genuinely cannot run and the reason is stated. Never reports OK over an
empty corpus.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

TIMEOUT = 20


def run(cmd, cwd=None, timeout=TIMEOUT, env=None):
    try:
        p = subprocess.run(cmd, cwd=cwd, timeout=timeout, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return p.returncode, p.stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        return "timeout", ""
    except OSError as e:
        return "oserror", str(e)


def build_and_run(cc, src, work, tag, cflags, opt):
    exe = os.path.join(work, "o_%s" % tag)
    ccf = cflags
    if os.name == "nt" and "clang" in os.path.basename(cc).lower():
        # clang on Windows defaults to the MSVC target, where the math
        # functions live in the CRT and `-lm` fails to link ("cannot open
        # m.lib"); gcc-mingw still needs `-lm`. Without this the clang oracle
        # nocompiles every program, qualified drops to 0 and the cross oracle
        # is vacuous on Windows. Unix and the gcc-mingw suite are unaffected.
        ccf = [f for f in cflags if f != "-lm"]
    rc, out = run([cc] + ccf + [opt, src, "-o", exe])
    if rc != 0:
        return ("nocompile", out[:400])
    rc, out = run([exe], cwd=work)
    try:
        os.unlink(exe)
    except OSError:
        pass
    if rc == "timeout":
        return ("timeout", "")
    if rc == "oserror":
        return ("norun", out[:200])
    h = hashlib.sha256(out.encode("utf-8", "replace")).hexdigest()[:16]
    return ("ok:%s" % h if rc == 0 else "fail:%s:%s" % (rc, h), out[:200])


def qualify(prog, args, work):
    base = os.path.basename(prog)[:-2]
    res = {}
    for name, cc in (("oracle", args.oracle_cc), ("suite", args.suite_cc)):
        for opt in ("-O0", "-O2"):
            v, _ = build_and_run(cc, prog, work, "%s_%s_%s" % (base, name, opt),
                                 args.cflags, opt)
            res["%s%s" % (name, opt)] = v
    vals = set(res.values())
    agreed = len(vals) == 1
    verdict = list(vals)[0] if agreed else "disagree"
    return res, agreed, verdict


def is_pass(v):
    return v.startswith("ok:")


NUM = re.compile(r"(\w[\w-]*)=(-?\d+)")

FUNNEL = ("funnel-seen", "funnel-refused", "funnel-extent-over-slot",
          "funnel-no-device-region", "funnel-cpu-reference-bailed",
          "funnel-no-device", "funnel-lower-no-live-in",
          "funnel-lower-nothing-to-run", "funnel-lower-f64-unsupported",
          "funnel-lower-emit-failed", "funnel-dispatch-failed",
          "funnel-disagreed", "funnel-agreed")


def slicerun_counts(text):
    d = {}
    for line in text.splitlines():
        if not line.startswith("slicerun:"):
            continue
        for k, v in NUM.findall(line):
            d[k] = int(v)
    return d


def one_program(idx, prog, args, root):
    work = os.path.join(root, "w%05d" % idx)
    os.makedirs(work, exist_ok=True)
    rec = {"prog": os.path.relpath(prog, args.corpus), "idx": idx}
    try:
        res, agreed, verdict = qualify(prog, args, work)
        rec["oracle"] = res
        rec["qualified"] = agreed and is_pass(verdict)
        rec["oracle_verdict"] = verdict

        dump = os.path.join(work, "arena.txt")
        env = dict(os.environ, MCC_ARENA_DUMP=dump)
        incs = [f for f in args.cflags if f.startswith("-I") or
                f.startswith("-D")]
        rc, out = run([args.mcc, "-c", prog, "-o", os.path.join(work, "a.o"),
                       "-O1"] + incs, env=env, timeout=60)
        rec["mcc_static"] = "ok" if rc == 0 else ("timeout" if rc == "timeout"
                                                  else "reject")
        rec["mcc_msg"] = out[:200] if rc != 0 else ""

        if not os.path.exists(dump) or os.path.getsize(dump) == 0:
            rec["bodies"] = 0
            rec["slices"] = 0
            return rec

        cref = os.path.join(work, "cref")
        os.makedirs(cref, exist_ok=True)
        cmd = [args.slicerun, "--arenas", dump, "--quiet",
               "--cref", cref, "--cref-prefix", "p%05dn" % idx]
        if args.mutate:
            cmd.append("--mutate")
        rc, out = run(cmd, timeout=180)
        c = slicerun_counts(out)
        rec.update({k: c.get(k, 0) for k in
                    ("bodies", "slices", "tuples", "gpu-slices", "dispatches",
                     "mismatches", "frame-accepted", "frame-built",
                     "frame-compared", "frame-mismatches", "cref-seen",
                     "cref-emitted", "cref-tuples", "cref-toobig",
                     "cref-unspellable", "cref-alldead",
                     "cref-mixed-operand-types") + FUNNEL})
        rec["slicerun_rc"] = rc
        rec["device"] = 1 if "available=1" in out else 0
        rec["cref_dir"] = cref if c.get("cref-emitted", 0) else None
    except Exception as e:  # noqa: BLE001
        rec["error"] = "%s: %s" % (type(e).__name__, e)
    return rec


def collect_cref(records, out_dir, jobs, cc_list, verbose):
    os.makedirs(out_dir, exist_ok=True)
    frags, seen = [], {}
    dup = 0
    for r in records:
        d = r.get("cref_dir")
        if not d or not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".c"):
                continue
            p = os.path.join(d, fn)
            with open(p, "r") as fh:
                text = fh.read()
            h = hashlib.sha256(text.encode()).hexdigest()
            if h in seen:
                dup += 1
                continue
            seen[h] = p
            frags.append(text)

    if not frags:
        return {"fragments": 0, "duplicates": dup, "batches": 0,
                "results": {}, "mismatches": []}

    batches = []
    BS = 150
    for i in range(0, len(frags), BS):
        chunk = frags[i:i + BS]
        tags = [re.search(r"static int chk_(\w+)\(void\)", c).group(1)
                for c in chunk]
        src = "#include <stdio.h>\n" + "\n".join(chunk)
        src += "\nint main(void) {\n\tint bad = 0;\n"
        for t in tags:
            src += "\tbad |= chk_%s();\n" % t
        src += "\treturn bad ? 1 : 0;\n}\n"
        path = os.path.join(out_dir, "batch%04d.c" % len(batches))
        with open(path, "w") as fh:
            fh.write(src)
        batches.append(path)

    results, mismatches = {}, []

    def one(cc, opt, path):
        exe = path[:-2] + "." + os.path.basename(cc) + opt.replace("-", "")
        rc, out = run([cc, "-w", "-fwrapv", "-fno-strict-overflow", opt,
                       path, "-o", exe], timeout=300)
        if rc != 0:
            return ("nocompile", path, out[:2000])
        rc, out = run([exe], timeout=120)
        try:
            os.unlink(exe)
        except OSError:
            pass
        return ("ok" if rc == 0 else "mismatch", path, out)

    for cc in cc_list:
        for opt in ("-O0", "-O2"):
            key = "%s%s" % (os.path.basename(cc), opt)
            nbad = nok = nnc = 0
            with ThreadPoolExecutor(max_workers=jobs) as ex:
                for st, path, out in ex.map(lambda p: one(cc, opt, p), batches):
                    if st == "ok":
                        nok += 1
                    elif st == "nocompile":
                        nnc += 1
                        mismatches.append({"oracle": key, "kind": "nocompile",
                                           "batch": path, "msg": out[:1200]})
                    else:
                        nbad += 1
                        for line in out.splitlines():
                            if line.startswith("MISMATCH"):
                                mismatches.append({"oracle": key,
                                                   "kind": "value",
                                                   "line": line})
            results[key] = {"ok": nok, "mismatch": nbad, "nocompile": nnc}
            if verbose:
                sys.stderr.write("cref %s: %s\n" % (key, results[key]))

    return {"fragments": len(frags), "duplicates": dup,
            "batches": len(batches), "results": results,
            "mismatches": mismatches}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--glob", default="*.c")
    ap.add_argument("--mcc", required=True)
    ap.add_argument("--slicerun", required=True)
    ap.add_argument("--oracle-cc", required=True,
                    help="the CROSS oracle: the compiler the corpus is NOT from")
    ap.add_argument("--suite-cc", required=True,
                    help="the compiler the corpus ships with")
    ap.add_argument("--cflags", default="-w,-std=gnu11,-fpermissive,-lm")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    ap.add_argument("--work")
    ap.add_argument("--json")
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--min-adjudicated", type=int, default=0)
    ap.add_argument("--min-cref-tuples", type=int, default=0)
    ap.add_argument("--expect-cref-mismatch", action="store_true",
                    help="known-positive: the run FAILS if the oracles agree")
    ap.add_argument("--recursive", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    a.cflags = [x for x in a.cflags.split(",") if x]

    if not os.path.isdir(a.corpus):
        sys.stderr.write("gpuconform: SKIP: no corpus at %s\n" % a.corpus)
        return 77
    for tool, what in ((a.mcc, "mcc"), (a.slicerun, "slicerun")):
        if not os.path.isfile(tool) or not os.access(tool, os.X_OK):
            sys.stderr.write("gpuconform: SKIP: no %s at %s\n" % (what, tool))
            return 77
    for cc in (a.oracle_cc, a.suite_cc):
        if shutil.which(cc) is None:
            sys.stderr.write("gpuconform: SKIP: oracle %s not on PATH\n" % cc)
            return 77

    progs = []
    for dirpath, _, files in os.walk(a.corpus):
        if not a.recursive and dirpath != a.corpus:
            continue
        for fn in sorted(files):
            if fn.endswith(".c"):
                progs.append(os.path.join(dirpath, fn))
    progs.sort()
    if a.limit:
        progs = progs[:a.limit]
    if not progs:
        sys.stderr.write("gpuconform: corpus %s holds no .c file; a run that "
                         "adjudicated nothing must not be reported as OK\n"
                         % a.corpus)
        return 1

    root = (os.path.abspath(a.work) if a.work
            else tempfile.mkdtemp(prefix="gpuconform-"))
    os.makedirs(root, exist_ok=True)

    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        records = list(ex.map(
            lambda t: one_program(t[0], t[1], a, root),
            list(enumerate(progs))))

    cref = collect_cref(records, os.path.join(root, "crefbatch"), a.jobs,
                        [a.oracle_cc, a.suite_cc], not a.quiet)

    q = [r for r in records if r.get("qualified")]
    classified_out = [r for r in records if not r.get("qualified")]
    withslice = [r for r in q if r.get("slices", 0) > 0]
    withgpu = [r for r in q if r.get("gpu-slices", 0) > 0]
    withdisp = [r for r in q if r.get("dispatches", 0) > 0]
    withmis = [r for r in q if r.get("mismatches", 0) > 0]
    device = any(r.get("device") for r in records)
    stalled = [r for r in records
               if r.get("slicerun_rc") in ("timeout", "oserror")]

    summary = {
        "corpus": a.corpus,
        "programs": len(progs),
        "qualified": len(q),
        "classified_out": len(classified_out),
        "classified_out_by_reason": {},
        "mcc_static_ok": sum(1 for r in q if r.get("mcc_static") == "ok"),
        "mcc_static_reject": sum(1 for r in q
                                 if r.get("mcc_static") == "reject"),
        "with_bodies": sum(1 for r in q if r.get("bodies", 0) > 0),
        "bodies_total": sum(r.get("bodies", 0) for r in q),
        "with_slice": len(withslice),
        "slices_total": sum(r.get("slices", 0) for r in q),
        "tuples_total": sum(r.get("tuples", 0) for r in q),
        "with_gpu_lowered": len(withgpu),
        "gpu_slices_total": sum(r.get("gpu-slices", 0) for r in q),
        "with_dispatch": len(withdisp),
        "dispatches_total": sum(r.get("dispatches", 0) for r in q),
        "with_device_mismatch": len(withmis),
        "frame_accepted": sum(r.get("frame-accepted", 0) for r in q),
        "frame_built": sum(r.get("frame-built", 0) for r in q),
        "frame_compared": sum(r.get("frame-compared", 0) for r in q),
        "frame_mismatches": sum(r.get("frame-mismatches", 0) for r in q),
        "device_present": device,
        "slicerun_stalled": len(stalled),
        "cref": cref,
    }
    for k in FUNNEL:
        summary[k.replace("-", "_")] = sum(r.get(k, 0) for r in q)
    _slcs = summary["slices_total"]
    summary["device_execution_fraction"] = (
        100.0 * summary["gpu_slices_total"] / _slcs) if _slcs else 0.0
    for r in classified_out:
        k = r.get("oracle_verdict", "?")
        k = k.split(":")[0] if k != "disagree" else "oracles-disagree"
        summary["classified_out_by_reason"][k] = \
            summary["classified_out_by_reason"].get(k, 0) + 1

    if a.json:
        with open(a.json, "w") as fh:
            json.dump({"summary": summary, "records": records}, fh, indent=1)

    out = sys.stdout
    out.write("gpuconform: corpus=%s programs=%d qualified=%d classified-out=%d\n"
              % (a.corpus, len(progs), len(q), len(classified_out)))
    out.write("gpuconform: funnel bodies=%d slices=%d tuples=%d gpu-slices=%d "
              "dispatches=%d device-mismatch-progs=%d\n"
              % (summary["bodies_total"], summary["slices_total"],
                 summary["tuples_total"], summary["gpu_slices_total"],
                 summary["dispatches_total"], summary["with_device_mismatch"]))
    out.write("gpuconform: device-execution-fraction=%.2f%% "
              "(%d of %d slices executed on device)\n"
              % (summary["device_execution_fraction"],
                 summary["gpu_slices_total"], summary["slices_total"]))
    out.write("gpuconform: progs with-bodies=%d with-slice=%d with-gpu=%d "
              "with-dispatch=%d\n"
              % (summary["with_bodies"], len(withslice), len(withgpu),
                 len(withdisp)))
    if stalled:
        out.write("gpuconform: slicerun did not finish on %d of %d program(s), "
                  "so every counter for them is zero because the tool never "
                  "reported and not because the funnel emitted nothing: %s\n"
                  % (len(stalled), len(progs),
                     " ".join(sorted(r["prog"] for r in stalled))[:400]))
    out.write("gpuconform: device present=%d dispatches=%d\n"
              % (1 if device else 0, summary["dispatches_total"]))
    out.write("gpuconform: frame accepted=%d built=%d compared=%d mismatches=%d\n"
              % (summary["frame_accepted"], summary["frame_built"],
                 summary["frame_compared"], summary["frame_mismatches"]))
    seen = summary["funnel_seen"]
    out.write("gpuconform: frame-funnel %s\n"
              % " ".join("%s=%d" % (k, summary[k.replace("-", "_")])
                         for k in FUNNEL))
    if seen:
        drops = [(k, summary[k.replace("-", "_")]) for k in FUNNEL[1:]]
        acc = seen - summary["funnel_refused"]
        out.write("gpuconform: frame-funnel accepted=%d of %d (%.2f%%), "
                  "agreed=%d (%.2f%% of accepted), drops-sum=%d\n"
                  % (acc, seen, 100.0 * acc / seen, summary["funnel_agreed"],
                     100.0 * summary["funnel_agreed"] / acc if acc else 0.0,
                     sum(v for _, v in drops)))
        for k, v in drops:
            if v and k not in ("funnel-refused", "funnel-agreed"):
                out.write("gpuconform: frame-drop %-28s n=%d share-of-accepted="
                          "%.2f%%\n" % (k, v, 100.0 * v / acc if acc else 0.0))
    out.write("gpuconform: cref fragments=%d dedup-dropped=%d tuples=%d "
              "mixed-operand-slices=%d\n"
              % (cref["fragments"], cref["duplicates"],
                 sum(r.get("cref-tuples", 0) for r in q),
                 sum(r.get("cref-mixed-operand-types", 0) for r in q)))
    for k, v in sorted(cref["results"].items()):
        out.write("gpuconform: cref-oracle %s ok=%d mismatch=%d nocompile=%d\n"
                  % (k, v["ok"], v["mismatch"], v["nocompile"]))
    for m in cref["mismatches"][:40]:
        out.write("gpuconform: CREF-DISAGREE %s\n" % json.dumps(m)[:600])

    bad = []
    nmis = sum(v["mismatch"] for v in cref["results"].values())

    nnc = sum(v["nocompile"] for v in cref["results"].values())
    if a.expect_cref_mismatch:
        if nmis == 0 and (nnc or not cref["results"]):
            sys.stderr.write("gpuconform: the known-positive arm could not be "
                             "adjudicated: %d oracle build(s) failed or timed "
                             "out over %d fragment(s), so this run says nothing "
                             "about whether the differential can fail. Retry on "
                             "a quiet machine before reading it as blindness\n"
                             % (nnc, cref["fragments"]))
            return 1
        if nmis == 0:
            sys.stderr.write("gpuconform: the mutated slice bodies still agreed "
                             "with the CPU reference under every oracle, so the "
                             "cref differential is blind\n")
            return 1
        out.write("gpuconform: known-positive OK, %d mutated batch(es) detected\n"
                  % nmis)
        return 0

    if stalled:
        bad.append("slicerun timed out or failed to run on %d program(s); every "
                   "funnel counter below is zero because the tool never reported"
                   % len(stalled))
    if len(q) < a.min_adjudicated:
        bad.append("adjudicated %d program(s) against both oracles, expected at "
                   "least %d; a harness whose corpus quietly stopped resolving "
                   "reports OK over nothing"
                   % (len(q), a.min_adjudicated))
    ctup = sum(r.get("cref-tuples", 0) for r in q)
    if ctup < a.min_cref_tuples:
        bad.append("the CPU reference answered %d tuple(s) that an oracle could "
                   "re-check, expected at least %d" % (ctup, a.min_cref_tuples))
    if a.min_cref_tuples and not cref["results"]:
        bad.append("no oracle compiled a single emitted slice, so the CPU "
                   "reference was compared against nothing")
    if nmis:
        bad.append("%d batch(es) of re-emitted slices disagreed with an oracle; "
                   "ast_eval_slice is a second implementation of C semantics and "
                   "a disagreement is a miscompile, not a coverage gap" % nmis)
    if summary["with_device_mismatch"]:
        bad.append("%d program(s) had a slice where the device disagreed with "
                   "the CPU reference" % summary["with_device_mismatch"])
    if summary["frame_mismatches"]:
        bad.append("%d frame run(s) disagreed between the CPU and the device"
                   % summary["frame_mismatches"])

    for b in bad:
        sys.stderr.write("gpuconform: %s\n" % b)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
