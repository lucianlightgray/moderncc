#!/bin/sh
# Link the amalgamation once per target define set. Catches a function that is
# CALLED under one target guard but DEFINED under a narrower one -- a link error
# that no native ctest cell can see, because ctest only ever builds the host
# target. mcc-x86_64-win32 broke exactly this way once.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
cc=${CC:-cc}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

inc="-I$root/src -I$root/include -I$root/src/formats -I$root/src/objfmt \
 -I$root/src/arch/i386 -I$root/src/arch/x86_64 -I$root/src/arch/arm \
 -I$root/src/arch/arm64 -I$root/src/arch/riscv64"

command -v "$cc" >/dev/null 2>&1 || { echo "no cc"; exit 77; }

fail=0
for t in \
  "-DMCC_TARGET_X86_64=1" \
  "-DMCC_TARGET_X86_64=1 -DMCC_TARGET_PE=1" \
  "-DMCC_TARGET_I386=1" \
  "-DMCC_TARGET_I386=1 -DMCC_TARGET_PE=1" \
  "-DMCC_TARGET_ARM64=1" \
  "-DMCC_TARGET_ARM64=1 -DMCC_TARGET_PE=1" \
  "-DMCC_TARGET_RISCV64=1" \
  "-DMCC_TARGET_ARM=1"
do
  if $cc -w -O0 -DMCC_CONFIG_OPTIMIZER=1 $t $inc -o "$work/m" "$root/src/mcc.c" -lm -ldl 2>"$work/err"; then
    echo "ok   $t"
  else
    echo "FAIL $t"
    head -5 "$work/err"
    fail=1
  fi
done
exit $fail
