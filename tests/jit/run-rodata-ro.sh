#!/bin/sh
# T-mac-30196 (Darwin): -run must make rodata read-only (write faults) while
# keeping .data/.bss writable — matching AOT.
MCC="$1"; SRCDIR="$2"
case "$(uname -s)" in Darwin) ;; *) echo "SKIP: -run rodata protection is Darwin-only"; exit 77;; esac
if "$MCC" -run "$SRCDIR/tests/jit/run_rwdata_write.c"; then
    echo "PASS: .data/.bss writable under -run"
else
    echo "FAIL: writable data not writable under -run (rc=$?)"; exit 1
fi
if "$MCC" -run "$SRCDIR/tests/jit/run_rodata_write.c" >/dev/null 2>&1; then
    echo "FAIL: const write under -run did not fault — rodata writable"; exit 1
else
    echo "PASS: const write faults under -run (rc=$?), matching AOT"
fi
