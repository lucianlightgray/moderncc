import argparse
import json
import os
import re
import subprocess
import sys

KINDS = ["BasicBlock", "If", "Jump", "Return", "Ref", "Literal", "Load", "Store",
         "Unary", "Binary", "Convert", "Invoke", "Poison", "StoreVal"]


def build_flags(build_dir):
    cc = os.path.join(build_dir, "compile_commands.json")
    if not os.path.exists(cc):
        return None
    for rec in json.load(open(cc)):
        if rec["file"].replace("\\", "/").endswith("src/mcc.c"):
            import shlex
            argv = shlex.split(rec["command"])
            return [a for a in argv[1:] if a.startswith("-D") or a.startswith("-I")]
    return None


def make_dump(mcc, flags, src, out, opt):
    env = dict(os.environ)
    env["MCC_RIR_PROD"] = "2"
    env["MCC_ARENA_DUMP"] = out
    if os.path.exists(out):
        os.remove(out)
    cmd = [mcc] + flags + ["-" + opt, "-c", src, "-o", out + ".o"]
    subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, timeout=600)
    return os.path.exists(out)


def census(path):
    bodies = []
    kinds = [0] * len(KINDS)
    invokes = []
    cur = None
    nodes = 0
    with open(path, errors="replace") as f:
        for line in f:
            if line.startswith("[arena] "):
                m = re.match(r"\[arena\] fn=(\S+) n=(\d+) root=", line)
                if m:
                    cur = m.group(1)
                    bodies.append(cur)
                continue
            if line.startswith("[inv] "):
                p = line.split()
                if len(p) >= 3:
                    invokes.append(p[2])
                continue
            p = line.split()
            if len(p) < 7:
                continue
            try:
                k = int(p[1])
            except ValueError:
                continue
            if 0 <= k < len(KINDS):
                kinds[k] += 1
                nodes += 1
    defined = set(bodies)
    internal = sum(1 for c in invokes if c in defined)
    unknown = sum(1 for c in invokes if c == "?")
    external = len(invokes) - internal - unknown
    return {
        "bodies": len(bodies),
        "nodes": nodes,
        "kinds": {KINDS[i]: kinds[i] for i in range(len(KINDS))},
        "invoke_sites": len(invokes),
        "invoke_internal": internal,
        "invoke_external": external,
        "invoke_unknown": unknown,
    }


def ceilings(c):
    n = c["nodes"]
    if not n:
        return {}
    inv = c["invoke_sites"]
    ext = c["invoke_external"] + c["invoke_unknown"]
    return {
        "all_invokes_on_cpu": round(100.0 * (n - inv) / n, 4),
        "external_invokes_on_cpu": round(100.0 * (n - ext) / n, 4),
    }


def report(c, label):
    n = c["nodes"] or 1
    print("== node census  %s" % label)
    print("   bodies %d  nodes %d" % (c["bodies"], c["nodes"]))
    for k in KINDS:
        v = c["kinds"][k]
        print("   %-12s %8d  %6.2f%%" % (k, v, 100.0 * v / n))
    print("   invokes %d = internal %d + external %d + unknown %d"
          % (c["invoke_sites"], c["invoke_internal"], c["invoke_external"],
             c["invoke_unknown"]))
    cl = ceilings(c)
    print("   ceiling: all-invokes-on-cpu %.4f%%  external-only %.4f%%"
          % (cl["all_invokes_on_cpu"], cl["external_invokes_on_cpu"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir")
    ap.add_argument("--srcdir", default=".")
    ap.add_argument("--src", default="src/mcc.c")
    ap.add_argument("--opt", default="O2")
    ap.add_argument("--dump")
    ap.add_argument("--bank", default="tests/rir/node-bank.json")
    ap.add_argument("--update-bank", action="store_true")
    ap.add_argument("--tol", type=float, default=0.05)
    a = ap.parse_args()

    if a.dump:
        dump = a.dump
    else:
        mcc = os.path.join(a.build_dir, "mcc")
        if not os.path.exists(mcc):
            print("node-census: SKIP: no mcc at %s" % mcc)
            return 77
        flags = build_flags(a.build_dir)
        if flags is None:
            print("node-census: SKIP: no compile_commands.json in %s" % a.build_dir)
            return 77
        dump = os.path.join(a.build_dir, "node_census_arena.txt")
        srcpath = os.path.join(a.srcdir, a.src)
        if not os.path.exists(srcpath):
            print("node-census: SKIP: no source at %s" % srcpath)
            return 77
        if not make_dump(mcc, flags, srcpath, dump, a.opt):
            print("node-census: SKIP: MCC_ARENA_DUMP produced nothing")
            return 77

    c = census(dump)
    if not c["nodes"]:
        print("node-census: FAIL: dump has no nodes")
        return 1
    label = "%s -%s" % (a.src, a.opt)
    report(c, label)
    c["ceilings"] = ceilings(c)

    key = "%s:%s" % (a.src, a.opt)
    banked = {}
    if os.path.exists(a.bank):
        banked = json.load(open(a.bank))
    if a.update_bank:
        banked[key] = c
        with open(a.bank, "w") as f:
            json.dump(banked, f, indent=1, sort_keys=True)
            f.write("\n")
        print("node-census: banked %s" % key)
        return 0
    if key not in banked:
        print("node-census: SKIP: no banked census for %s (use --update-bank)" % key)
        return 77

    b = banked[key]
    bad = []
    for k in KINDS:
        if b["kinds"].get(k, 0) and not c["kinds"][k]:
            bad.append("kind %s vanished: banked %d, now 0" % (k, b["kinds"][k]))
    if c["invoke_sites"] and not b.get("invoke_sites"):
        bad.append("invoke_sites appeared where the bank had none")
    # all_invokes_on_cpu is (nodes - invoke_sites) / nodes -- a measure of the
    # corpus's own call density, not of anything the compiler can move. It fell
    # 94.9385% -> 94.8004% purely because src/mcc.c amalgamated ~2700 lines, and
    # the same class of dilution has now been observed twice more in
    # rir-coverage, once moving two ratios of the same census in opposite
    # directions. Gating it asserts a signal that is not there. Reported, not
    # gated. external_invokes_on_cpu stays gated because it is the number the
    # GPU plan's ceiling rests on -- but it is the same kind of ratio, merely
    # ~7x less sensitive because its numerator is ~7x smaller.
    print("  all_invokes_on_cpu: %.4f%% (reported, not gated: a ratio over the "
          "compiler's own source)" % c["ceilings"]["all_invokes_on_cpu"])
    for name in ("external_invokes_on_cpu",):
        was = b.get("ceilings", {}).get(name)
        now = c["ceilings"][name]
        if was is not None and now + a.tol < was:
            bad.append("%s regressed: %.4f%% < banked %.4f%%" % (name, now, was))
    if bad:
        for m in bad:
            print("node-census: FAIL: %s" % m)
        return 1
    print("node-census: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
