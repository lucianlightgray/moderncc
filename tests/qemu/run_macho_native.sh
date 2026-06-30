#!/bin/sh
# Native Mach-O (Darwin) RUNTIME conformance — for an actual macOS host.
#
# The codegen-run / image-run / apple-libc drivers exist to approximate, on a
# Linux/x86_64 host, what a real macOS host gives for free: a loader, a real
# dyld, real libSystem, working TLV. On Darwin none of that scaffolding is
# needed — the native `mcc` already emits a native Mach-O executable linked
# against the system libSystem, so each self-checking conformance program just
# compiles and runs. This therefore covers a STRICT SUPERSET of the Linux
# approximations: the TLS programs (Darwin TLV thunks the ELF shortcut can't
# provide) and the libc-dependent programs (real locale/stdio/malloc) all run
# here, where target == host.
#
# Usage: run_macho_native.sh <src-dir> <mcc> <bdir> <work-dir>
# Self-skips (exit 77, ctest SKIP_RETURN_CODE) off a Darwin host or without a
# Mach-O-targeting mcc.
set -eu

SRC=$1; MCC=$2; BDIR=$3; WORK=$4
CONF="$SRC/tests/qemu/conformance"
INC="$SRC/runtime/include"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: host is not Darwin (native Mach-O needs a macOS host)"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no native mcc ($MCC)"; exit 77; }
# Confirm this mcc actually targets Mach-O (a cross build on macOS could target
# ELF/PE, in which case the produced binary won't run on the host).
mkdir -p "$WORK"
printf 'int main(void){return 0;}\n' > "$WORK/probe.c"
if ! "$MCC" "-B${BDIR}" "$WORK/probe.c" -o "$WORK/probe" 2>"$WORK/probe.err"; then
    echo "SKIP: native mcc cannot link an executable: $(head -1 "$WORK/probe.err" 2>/dev/null)"; exit 77
fi
case "$(file -b "$WORK/probe" 2>/dev/null)" in
    Mach-O*) ;;
    *) echo "SKIP: native mcc does not target Mach-O ($(file -b "$WORK/probe" 2>/dev/null))"; exit 77;;
esac

# The full self-checking set. Unlike the Linux drivers, nothing here is excluded
# for want of a loader/libc/TLV: tls* exercise Darwin TLV, and the *_libc / libc*
# programs exercise the real macOS C library. Each returns 0 on success.
PROGS="atomics control integers floats lexical aggregates varargs complex_annexg
       control_libc floats_libc libc libc_struct varargs_fp vla tls tls_aggr"

status=0
for t in $PROGS; do
    [ -f "$CONF/$t.c" ] || { echo "FAIL $t (missing source)"; status=1; continue; }
    if ! "$MCC" "-B${BDIR}" "-I${INC}" "$CONF/$t.c" -o "$WORK/$t" 2>"$WORK/$t.err"; then
        echo "FAIL osx/$t (compile): $(head -1 "$WORK/$t.err" 2>/dev/null)"; status=1; continue
    fi
    case "$(file -b "$WORK/$t" 2>/dev/null)" in
        Mach-O*) ;;
        *) echo "FAIL osx/$t: not a Mach-O image"; status=1; continue;;
    esac
    if "$WORK/$t" >/dev/null 2>&1; then
        echo "PASS osx/$t (native Mach-O executed)"
    else
        echo "FAIL osx/$t (run, rc=$?)"; status=1
    fi
    rm -f "$WORK/$t"
done
exit $status
