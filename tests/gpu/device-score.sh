#!/bin/sh
# T-lin-10035: the physical device is chosen by score (mcc_vk_device_score),
# not by taking devs[0]. On a host that enumerates two or more usable devices
# this asserts the device the ladder actually initialised is the highest-scored
# one; on a host with fewer than two there is nothing to choose between, so it
# skips honestly (77). Non-blind: with the pre-ae344467 devs[0] selection the
# chosen score is the first enumerated device's, not the max, and this fails --
# demonstrated by fault-injecting "pick the first valid device" into the current
# tree, where the cell reports chosen 4564 against a max of 5564.
set -u

RUNNER=${1:-}
[ -n "$RUNNER" ] && [ -x "$RUNNER" ] || {
	echo "SKIP device-score: no slicerun at '$RUNNER'"
	exit 77
}

out=$(MCC_AST_EVAL_LADDER_GPU_DIAG=1 "$RUNNER" gpu 2>&1)

ndev=$(printf '%s\n' "$out" | grep -c '^\[gpu-vk\] device ')
[ "$ndev" -ge 2 ] || {
	echo "SKIP device-score: $ndev enumerated device(s), need >=2 to test the choice"
	exit 77
}

maxsc=$(printf '%s\n' "$out" |
	sed -n 's/^\[gpu-vk\] device .* score=\(-\{0,1\}[0-9]*\).*$/\1/p' |
	sort -n | tail -1)
chosc=$(printf '%s\n' "$out" |
	sed -n 's/^\[ladder-gpu\] init ok dev=.* score=\(-\{0,1\}[0-9]*\).*$/\1/p' |
	head -1)
[ -n "$chosc" ] || {
	echo "SKIP device-score: the ladder did not initialise a device"
	exit 77
}

if [ "$chosc" = "$maxsc" ]; then
	echo "device-score: OK -- chose the score=$chosc device, the max of $ndev enumerated"
	exit 0
fi

echo "FAIL device-score: chosen score=$chosc but the max enumerated score is $maxsc (devs[0] regression?)" >&2
printf '%s\n' "$out" | grep -E '^\[gpu-vk\] device |^\[ladder-gpu\] init ok' >&2
exit 1
