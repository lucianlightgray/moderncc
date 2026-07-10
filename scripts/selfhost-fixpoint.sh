#!/bin/sh
# 3-stage self-host fixpoint: <build>/mcc compiles src/mcc.c -> stage2,
# stage2 -> stage3, stage3 -> stage4; all three must be byte-identical.
# Defines are read from <build>/compile_commands.json so this tracks the
# build's real config. Usage: scripts/selfhost-fixpoint.sh [build-dir]
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
B=${1:-$R/cmake-debug}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
DEFS=$(python3 - "$B/compile_commands.json" <<'PY'
import json,sys,shlex
for e in json.load(open(sys.argv[1])):
    if e['file'].endswith('src/mcc.c') and '/mcc.dir/' in e['output']:
        out=[]
        for a in shlex.split(e['command']):
            if a.startswith('-D') and 'MCC_EMBED_MCCRT' not in a:
                out.append(a)
        print(' '.join(shlex.quote(x) for x in out)); break
PY
)
INCS="-I$B -I$R/src -I$R/src/arch/i386 -I$R/src/arch/x86_64 -I$R/src/arch/arm -I$R/src/arch/arm64 -I$R/src/arch/riscv64 -I$R/src/objfmt -I$R/src/formats -I$R/include -I$R"
eval "\"$B/mcc\" -B\"$B\" \"$R/src/mcc.c\" -o \"$TMP/stage2\" $INCS $DEFS"
eval "\"$TMP/stage2\" -B\"$B\" \"$R/src/mcc.c\" -o \"$TMP/stage3\" $INCS $DEFS"
eval "\"$TMP/stage3\" -B\"$B\" \"$R/src/mcc.c\" -o \"$TMP/stage4\" $INCS $DEFS"
cmp "$TMP/stage2" "$TMP/stage3" && cmp "$TMP/stage3" "$TMP/stage4" && echo "FIXPOINT OK (stage2==stage3==stage4)"
