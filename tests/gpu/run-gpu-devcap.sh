#!/bin/sh
set -e
RUN=$1
[ -n "$RUN" ] || { echo "usage: run-gpu-devcap.sh <slicerun>" >&2; exit 2; }

held() {
	if [ -n "$1" ]; then
		out=$(MCC_GPU_MAX_DEVICES="$1" "$RUN" route --device-or-skip 2>&1) || _rc=$?
	else
		out=$("$RUN" route --device-or-skip 2>&1) || _rc=$?
	fi
	if [ -n "${_rc:-}" ] && [ "${_rc:-0}" -ne 0 ]; then
		if [ "$_rc" = 77 ]; then echo skip; unset _rc; return; fi
		echo "FAIL gpu-devcap: slicerun route exited $_rc:" >&2
		echo "$out" >&2
		exit 1
	fi
	printf '%s' "$out" | sed -n 's/.*route over \([0-9][0-9]*\) held device.*/\1/p'
}

phys=$(held "")
if [ "$phys" = skip ]; then
	echo "gpu-devcap: SKIP -- no usable Vulkan device"
	exit 77
fi
if [ -z "$phys" ] || [ "$phys" -lt 1 ]; then
	echo "FAIL gpu-devcap: could not read physical held-device count (got '$phys')" >&2
	exit 1
fi

one=$(held 1)
if [ "$one" != 1 ]; then
	echo "FAIL gpu-devcap: MCC_GPU_MAX_DEVICES=1 held $one, want 1" >&2
	exit 1
fi

big=$(held $((phys + 3)))
if [ "$big" != "$phys" ]; then
	echo "FAIL gpu-devcap: MCC_GPU_MAX_DEVICES=$((phys + 3)) held $big, want $phys (must clamp to physical)" >&2
	exit 1
fi

if [ "$phys" -ge 2 ]; then
	two=$(held 2)
	if [ "$two" != 2 ]; then
		echo "FAIL gpu-devcap: MCC_GPU_MAX_DEVICES=2 held $two, want 2" >&2
		exit 1
	fi
	if [ "$one" -ge "$phys" ]; then
		echo "FAIL gpu-devcap: cap=1 did not reduce below physical $phys (the cap is a no-op)" >&2
		exit 1
	fi
	echo "gpu-devcap: OK -- multi-GPU physical=$phys; cap=1 reduced to 1, cap=2 held 2, cap>=phys clamped to $phys"
else
	echo "gpu-devcap: OK -- single-GPU physical=1; cap=1 held 1, cap>=1 clamped to 1"
fi
