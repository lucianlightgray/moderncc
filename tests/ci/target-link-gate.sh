#!/bin/sh
# Extra arguments are appended to every link line. CMake passes the compute
# backend the build is configured for -- the -DMCC_GPU_LANG_MSL= that selects
# the mccgpu.c arm and the library that arm needs -- because src/mcc.c compiles
# the GPU backend unconditionally now, and this gate builds it outside the
# CMake target that would otherwise carry both.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
cc=${CC:-cc}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

inc="-I$root/src -I$root/include -I$root/src/formats -I$root/src/objfmt \
 -I$root/src/arch/i386 -I$root/src/arch/x86_64 -I$root/src/arch/arm \
 -I$root/src/arch/arm64 -I$root/src/arch/riscv64"

command -v "$cc" >/dev/null 2>&1 || {
  echo "SKIP: target-link-gate needs a host C compiler; '\$CC' resolved to '$cc', which is not on PATH"
  exit 77
}

objcgate=0
if [ "$(uname -s)" = Darwin ] && command -v nm >/dev/null 2>&1; then
  objcgate=1
else
  echo "skip objc-undef gate (not Darwin, or no nm)"
fi

objcpat='objc_|sel_registerName'

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
  if $cc -w -O0 -DMCC_CONFIG_OPTIMIZER=1 $t $inc -o "$work/m" "$root/src/mcc.c" -lm -ldl "$@" 2>"$work/err"; then
    if [ "$objcgate" = 1 ] && nm -u "$work/m" 2>/dev/null | grep -Eq "$objcpat"; then
      echo "FAIL $t  undefined Objective-C runtime symbols; src/mcc.c must not need -lobjc"
      nm -u "$work/m" | grep -E "$objcpat" | sed 's/^/       /'
      fail=1
    else
      echo "ok   $t"
    fi
  else
    echo "FAIL $t"
    head -5 "$work/err"
    fail=1
  fi
done
exit $fail
