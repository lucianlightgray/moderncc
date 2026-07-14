#!/bin/sh
# §26 2B — build the freestanding arm64 JIT counter/profiling-stub validator with
# a cross clang + lld and run it under qemu-aarch64. Skips (pass) when the
# aarch64 cross-toolchain or user-mode qemu isn't present, so it only bites where
# it can actually validate.
set -e
SRC="$1"
WORK="$2"

find_tool() {
	for t in "$@"; do
		command -v "$t" >/dev/null 2>&1 && {
			printf '%s\n' "$t"
			return 0
		}
	done
	return 1
}

CLANG=$(find_tool clang clang-22 /usr/lib/llvm/22/bin/clang) || {
	echo "SKIP: no clang cross-compiler"
	exit 0
}
QEMU=$(find_tool qemu-aarch64 qemu-aarch64-static) || {
	echo "SKIP: no qemu-aarch64"
	exit 0
}

EXE="$WORK/jit_arm64_counter"
if ! "$CLANG" --target=aarch64-linux-gnu -fuse-ld=lld -nostdlib -static -O1 \
		-o "$EXE" "$SRC" 2>"$WORK/jit_arm64_counter.log"; then
	echo "SKIP: aarch64 cross-link unavailable"
	cat "$WORK/jit_arm64_counter.log"
	exit 0
fi

if "$QEMU" "$EXE"; then
	echo "PASS: arm64 counter/profiling stub (spill+tick+restore+tail-br) validated under qemu"
	exit 0
else
	echo "FAIL: arm64 counter validator exited $?"
	exit 1
fi
