#!/usr/bin/env python3
"""How much of the RIR arena carries no static type, by node kind.

The lowerable census asks whether a node is a shader-lowering candidate; this
asks the prior question -- does the node even say what value it produces? An
equivalence oracle cannot bit-match a value whose width and signedness are
unknown, so this is the denominator that has to go to zero first.

Two columns, and the difference matters: `unknown` is a node whose static type
was never recorded, `knownvoid` is a node that provably yields no value.
VT_VOID is 0, so `type_t == 0` cannot tell them apart -- ast_stype_known()
does, via the arena's separate st_have bit.

Usage:
  tools/untyped-probe.py <build-dir> [O0,O1,O2,O3] [self|wide]
  PROBE_ENV="MCC_RIR_STAMP=2" tools/untyped-probe.py <build-dir>

Reads the `[rir-untyped]` / `[rir-untyped-kind]` lines the compiler emits under
MCC_RIR_PROD=2 (src/mccrir.c rir_prod_report).
"""
import json, os, shlex, subprocess, sys, tempfile, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
bdir = sys.argv[1]
levels = (sys.argv[2] if len(sys.argv) > 2 else "O0,O1,O2,O3").split(",")
corpus = sys.argv[3] if len(sys.argv) > 3 else "self"
extra = os.environ.get("PROBE_ENV", "")

cc = json.load(open(os.path.join(bdir, "compile_commands.json")))
rec = [x for x in cc if x["file"].endswith("/mcc.c")][0]
flags = [a for a in shlex.split(rec["command"])[1:]
         if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]
mcc = os.path.join(bdir, "mcc")

if corpus == "self":
    srcs = [os.path.join(ROOT, "src", "mcc.c")]
else:
    srcs = [os.path.join(ROOT, "src", "mcc.c")]
    for d in ("exec", "behavior", "ast", "asm", "runtime", "static"):
        srcs += sorted(glob.glob(os.path.join(ROOT, "tests", d, "**", "*.c"),
                                 recursive=True))
    srcs += sorted(glob.glob(os.path.join(ROOT, "examples", "*.c")))

for lvl in levels:
    tot = {}
    n = u = v = 0
    for src in srcs:
        env = dict(os.environ)
        env["MCC_RIR_PROD"] = "2"
        if lvl == "O0":
            env["MCC_FORCE_REPLAY"] = "1"
        for kv in extra.split():
            ek, _, ev = kv.partition("=")
            env[ek] = ev
        with tempfile.NamedTemporaryFile(suffix=".o", delete=False) as f:
            out_o = f.name
        p = subprocess.run([mcc] + flags + ["-" + lvl, "-c", src, "-o", out_o],
                           capture_output=True, text=True, cwd=ROOT, env=env)
        os.unlink(out_o)
        for line in p.stderr.splitlines():
            f2 = line.split()
            if not f2:
                continue
            if f2[0] == "[rir-untyped]":
                n += int(f2[1].split("=")[1])
                u += int(f2[2].split("=")[1])
                v += int(f2[3].split("=")[1])
            elif f2[0] == "[rir-untyped-kind]":
                k = f2[1]
                a, b, c2 = (int(f2[2].split("=")[1]), int(f2[3].split("=")[1]),
                            int(f2[4].split("=")[1]))
                t = tot.setdefault(k, [0, 0, 0])
                t[0] += a
                t[1] += b
                t[2] += c2
    print("== -%s  nodes=%d unknown=%d %.3f%%  knownvoid=%d %.3f%%" %
          (lvl, n, u, 100.0 * u / n if n else 0.0, v,
           100.0 * v / n if n else 0.0))
    for k in sorted(tot, key=lambda x: -tot[x][1]):
        a, b, c2 = tot[k]
        print("   %-12s n=%-8d unknown=%-8d %6.3f%% of all   knownvoid=%-8d"
              % (k, a, b, 100.0 * b / n if n else 0.0, c2))
    sys.stdout.flush()
