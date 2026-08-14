#!/bin/sh
# The process-lifetime bracket: src/mccrt.h.
#
# The GPU device and the JIT worker pool are process-wide resources. They used
# to start lazily and stop from atexit handlers registered by whichever lazy
# path ran first, which the tree's own comments record as producing an
# accidental teardown order and a teardown that races the driver's unload. They
# now start at main's entry and stop at its exit.
#
# Three properties, each of which has actually been broken:
#
#   1. An ordinary compile never brings a device up. The boot is opt-in, and a
#      bracket that booted unconditionally would put a Vulkan device creation in
#      front of every compile in every build.
#
#   2. The env opt-in boots. This is the path that runs at main entry.
#
#   3. The CLI-forced boot still happens. This is the subtle one:
#      ast_ladder_gpu_setup() marks itself `done` once, and it used to do so
#      BEFORE checking whether anyone had asked. Called at main entry -- where
#      argv has not been parsed and no force flag has arrived -- that marked it
#      done and silently swallowed the later forced boot. Measured: with the old
#      guard, `--jit-always-gpu` produced no [ladder-gpu] line at all, because
#      the hook was never installed. Declining because nobody asked must not
#      count as done.
set -eu

MCC=$1
WORK=$2

[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"
src=$WORK/hello.c
cat >"$src" <<'EOF'
extern int printf(const char *, ...);
int main(void) { printf("hi\n"); return 0; }
EOF

rc=0

# 1. Silent by default.
n=$("$MCC" -w "$src" -o "$WORK/a.out" 2>&1 | grep -c 'ladder-gpu' || true)
if [ "${n:-0}" -ne 0 ]; then
	echo "FAIL gpu-lifecycle: an ordinary compile touched the GPU ($n line(s))."
	echo "  The boot is opt-in; booting unconditionally at main entry would put a"
	echo "  device creation in front of every compile."
	rc=1
fi
# And it must still produce a working program.
if ! "$WORK/a.out" >"$WORK/out.txt" 2>&1 || [ "$(cat "$WORK/out.txt")" != "hi" ]; then
	echo "FAIL gpu-lifecycle: the plain compile did not produce a working program"
	rc=1
fi

# 2 and 3. Both request paths must reach a boot. `tried=1` is the assertion;
# `available` depends on the host having a device and is deliberately not
# asserted, so this cell means the same thing on a machine without one.
for how in env cli; do
	if [ "$how" = env ]; then
		out=$(MCC_AST_EVAL_LADDER_GPU=1 "$MCC" -w "$src" -o "$WORK/b.out" 2>&1 || true)
	else
		out=$("$MCC" -w --jit-always-gpu "$src" -o "$WORK/b.out" 2>&1 || true)
	fi
	case $out in
	*"[ladder-gpu] tried=1"*) ;;
	*"[ladder-gpu]"*)
		echo "FAIL gpu-lifecycle: the $how request reported a [ladder-gpu] line but"
		echo "  not tried=1, so the setup ran without attempting a device:"
		printf '%s\n' "$out" | grep 'ladder-gpu' | sed -e 's/^/    /'
		rc=1
		;;
	*)
		echo "FAIL gpu-lifecycle: the $how request produced NO [ladder-gpu] line at"
		echo "  all, so ast_ladder_gpu_setup() declined and left the hook uninstalled."
		echo "  For the cli case this is the exact regression the 'done' guard causes"
		echo "  when it marks itself done before checking whether anyone asked."
		rc=1
		;;
	esac
done

[ "$rc" -eq 0 ] || exit 1
echo "gpu-lifecycle: OK (silent by default; env and CLI requests both boot)"
exit 0
