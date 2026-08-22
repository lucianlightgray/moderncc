#!/bin/sh
set -e
RUN=$1
[ -n "$RUN" ] || { echo "usage: run-gpu-vrambudget.sh <slicerun>" >&2; exit 2; }

probe=$("$RUN" route --device-or-skip 2>&1) || {
	rc=$?
	[ "$rc" = 77 ] && { echo "gpu-vrambudget: SKIP -- no usable Vulkan device"; exit 77; }
	echo "FAIL gpu-vrambudget: slicerun route exited $rc:" >&2
	echo "$probe" >&2
	exit 1
}

diag() {
	if [ -n "$1" ]; then
		MCC_GPU_VRAM_BUDGET="$1" MCC_AST_EVAL_LADDER_GPU_DIAG=1 "$RUN" route --device-or-skip 2>&1 |
			sed -n 's/.*slot 0 .*maxsbrange=\([0-9]*\) freevram=\([0-9]*\) maxsb=\([0-9]*\).*/\1 \2 \3/p' | head -1
	else
		MCC_AST_EVAL_LADDER_GPU_DIAG=1 "$RUN" route --device-or-skip 2>&1 |
			sed -n 's/.*slot 0 .*maxsbrange=\([0-9]*\) freevram=\([0-9]*\) maxsb=\([0-9]*\).*/\1 \2 \3/p' | head -1
	fi
}

set -- $(diag "")
msr=$1; fv=$2; maxsb=$3
if [ -z "$maxsb" ] || [ "$maxsb" -lt 1 ]; then
	# The slot budget + VK_EXT_memory_budget free-VRAM reading this cell checks
	# is Vulkan-specific. A Metal device's route reports only maxBufferLength
	# (no slot/maxsb diag), so there is nothing to exercise here -- skip rather
	# than fail (T-mac-30293).
	if MCC_AST_EVAL_LADDER_GPU_DIAG=1 "$RUN" route --device-or-skip 2>&1 | grep -q 'maxBufferLength='; then
		echo "gpu-vrambudget: SKIP -- Metal device (Vulkan-only VK_EXT_memory_budget slot-budget test)"
		exit 77
	fi
	echo "FAIL gpu-vrambudget: could not read the slot-0 budget diag" >&2
	exit 1
fi
if [ "$msr" != "$maxsb" ]; then
	echo "FAIL gpu-vrambudget: default maxsbrange=$msr, want the device limit maxsb=$maxsb" >&2
	exit 1
fi

set -- $(diag 50)
msr50=$1; maxsb50=$3
half=$(( maxsb50 / 2 ))
delta=$(( msr50 - half )); [ "$delta" -lt 0 ] && delta=$(( -delta ))
if [ "$delta" -gt 1 ]; then
	echo "FAIL gpu-vrambudget: 50% maxsbrange=$msr50, want ~$half (maxsb/2)" >&2
	exit 1
fi
if [ "$msr50" -ge "$maxsb50" ]; then
	echo "FAIL gpu-vrambudget: 50% did not reduce below maxsb=$maxsb50 (budget is a no-op)" >&2
	exit 1
fi

set -- $(diag auto)
msra=$1; fva=$2; maxsba=$3
if [ "$fva" -lt 1 ]; then
	echo "FAIL gpu-vrambudget: auto read free VRAM as 0 -- VK_EXT_memory_budget not working" >&2
	exit 1
fi
if [ "$fva" -lt "$maxsba" ]; then want=$fva; else want=$maxsba; fi
if [ "$msra" != "$want" ]; then
	echo "FAIL gpu-vrambudget: auto maxsbrange=$msra, want min(maxsb=$maxsba, free=$fva)=$want" >&2
	exit 1
fi

echo "gpu-vrambudget: OK -- default=maxsb($maxsb); 50%=$msr50 (reduced below $maxsb50); auto=$msra=min(maxsb=$maxsba,free=$fva) via VK_EXT_memory_budget"
