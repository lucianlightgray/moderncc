#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys

src, bdir, cc = sys.argv[1], sys.argv[2], sys.argv[3]
work = os.path.join(bdir, "eval-binop-oracle-kp")
shutil.rmtree(work, ignore_errors=True)
os.makedirs(os.path.join(work, "src"))
os.makedirs(os.path.join(work, "tools"))

hdr_path = os.path.join(src, "src", "ast_eval_slice.h")
hdr = open(hdr_path).read()

want = "\t\t\tr = s;\n\t\t}\n\t\tbreak;\n\tcase '-':"
if hdr.count(want) != 1:
    sys.exit("eval-binop-oracle-known-positive: the 32-bit signed '+' arm of "
             "ast_eval_binop() no longer ends with the text this mutation "
             "rewrites (%d match(es)), so nothing would be perturbed and a "
             "green result would mean the oracle was never tested. Re-read the "
             "arm and update the anchor." % hdr.count(want))

mut = hdr.replace(want, "\t\t\tr = s + 1;\n\t\t}\n\t\tbreak;\n\tcase '-':")
open(os.path.join(work, "src", "ast_eval_slice.h"), "w").write(mut)
shutil.copyfile(os.path.join(src, "tools", "eval_binop_oracle.c"),
                os.path.join(work, "tools", "eval_binop_oracle.c"))

exe = os.path.join(work, "oracle")
b = subprocess.run([cc, "-O1", "-w",
                    "-I", os.path.join(src, "src"),
                    os.path.join(work, "tools", "eval_binop_oracle.c"),
                    "-o", exe], capture_output=True, text=True)
if b.returncode != 0:
    sys.exit("eval-binop-oracle-known-positive: the mutated tree did not build, "
             "so the oracle never ran and this proves nothing:\n" + b.stderr)

r = subprocess.run([exe], capture_output=True, text=True)
if r.returncode == 0:
    sys.exit("eval-binop-oracle-known-positive: ast_eval_binop()'s 32-bit "
             "signed '+' arm was made to return s + 1 and the oracle still "
             "passed. That is the exact fault N7 recorded as changing nothing "
             "observable across six engines, 1782 dump rows and eleven smoke "
             "cells; an oracle that cannot see it is measuring nothing:\n"
             + r.stdout)

if "op=43" not in r.stdout:
    sys.exit("eval-binop-oracle-known-positive: the oracle went red, but not on "
             "the '+' opcode that was perturbed, so it failed for some other "
             "reason and this is not the proof it claims to be:\n" + r.stdout)

print("eval-binop-oracle-known-positive: OK -- the injected r = s + 1 is caught "
      "on '+', first case tried")
