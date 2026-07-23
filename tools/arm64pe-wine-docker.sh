#!/bin/sh
# Run an mcc arm64-Windows (arm64-PE) compiler's OUTPUT under the native ARM64
# PE loader of wine-arm64, inside a --platform linux/arm64 Docker container, on
# any host (validated on x86_64 via qemu-user emulation).
#
# mcc runs on the host and emits an arm64 PE exe; wine-arm64 in the container
# loads and runs it. Debian bookworm's `wine` (8.0) ships the native AArch64 PE
# loader, so a genuine arm64-PE exe runs — the `wine: failed to open
# ...syswow64\\rundll32.exe: c0000135` line during first-boot prefix setup is
# only the non-fatal ARM32/WoW stub, not the arm64 loader, and is filtered out.
#
# What it validates (the CODEGEN / LOGIC portion of the arm64-PE runtime JIT):
#   1. hello       — a trivial arm64-PE exe runs at all (msvcrt puts, rc=0).
#   2. jit-dispatch — the mode-6 dispatch-slot + pointer-swap FRAMELESS-LEAF
#      return path, emitted as the exact stub shape mccjit_patch_make_slot uses
#      (movz/movk x17,#slot ; ldr x16,[x17] ; br x16), swapped between two
#      targets and re-dispatched, with an FP-returning target too. This is the
#      "frameless-leaf return corruption" mechanism — its LOGIC runs correctly
#      here; only the NATIVE-FAULT class (icache coherence on real silicon,
#      RtlAddFunctionTable unwind walking, wild-jump faults that spin-not-fault
#      under qemu's x86-TSO) genuinely needs arm64-Windows HW.
#
# Usage:  tools/arm64pe-wine-docker.sh <mcc-arm64-win32> <mccdir> [workdir]
#   <mcc-arm64-win32> : the cross compiler (host-runnable, emits arm64 PE)
#   <mccdir>          : a staged win32 sysroot (its lib/ has
#                       arm64-win32-libmccrt.a + *.def, include/ the headers);
#                       pass to mcc via -B. Build the archive with the
#                       `arm64-win32-mccrt` CMake target.
# Exit:  0 all pass · 1 a check failed · 77 skipped (no docker / arm64 / inputs)

set -eu
. "$(dirname "$0")/dockergate.sh"

MCC="${1:-}"
MCCDIR="${2:-}"
WORK="${3:-./w-arm64pe-wine}"
IMAGE="${MCC_ARM64_WINE_IMAGE:-mcc-wine-arm64:local}"
BASE_IMAGE="${MCC_ARM64_WINE_BASE:-debian:bookworm-slim}"

dg_need_bin "$MCC" "arm64-win32 mcc"
if [ -z "$MCCDIR" ] || [ ! -d "$MCCDIR" ]; then
	echo "SKIP: arm64-win32 mccdir sysroot not found at '${MCCDIR:-<unset>}'"
	exit 77
fi
dg_need_docker
dg_need_platform linux/arm64 "$BASE_IMAGE"

# Build (once) a cached wine-arm64 image so re-runs don't re-apt under emulation.
if ! dg_docker image inspect "$IMAGE" >/dev/null 2>&1; then
	echo "== building cached wine-arm64 image $IMAGE (first run; slow under emulation) =="
	tmpd=$(mktemp -d)
	cat > "$tmpd/Dockerfile" <<EOF
FROM $BASE_IMAGE
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y -qq wine && rm -rf /var/lib/apt/lists/*
ENV WINEDEBUG=-all
EOF
	if ! dg_docker buildx build --platform linux/arm64 --load -t "$IMAGE" "$tmpd" >/dev/null 2>&1; then
		rm -rf "$tmpd"
		echo "SKIP: could not build wine-arm64 image (no wine for arm64?)"
		exit 77
	fi
	rm -rf "$tmpd"
fi

rm -rf "$WORK"
mkdir -p "$WORK"
WORK_ABS=$(cd "$WORK" && pwd)

cat > "$WORK_ABS/hello.c" <<'EOF'
int puts(const char *);
int main(void){ puts("hello-arm64-pe"); return 0; }
EOF

cat > "$WORK_ABS/jitdisp.c" <<'EOF'
/* mode-6 dispatch-slot + pointer-swap frameless-leaf return path, exercised on
   the real arm64-PE loader. Emits the exact stub mccjit_patch_make_slot uses. */
typedef unsigned long long u64;
typedef unsigned int u32;
void *VirtualAlloc(void *, unsigned long long, unsigned long, unsigned long);
int VirtualProtect(void *, unsigned long long, unsigned long, unsigned long *);
void *GetCurrentProcess(void);
int FlushInstructionCache(void *, const void *, unsigned long long);
int printf(const char *, ...);
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE_READ 0x20
static int target1(int x){ return x + 111; }
static int target2(int x){ return x + 222; }
static double fmul(double a, double b){ return a * b + a; }
static void *mkslot(void **slot, void *target){
	unsigned char *p = VirtualAlloc(0, 4096, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
	u64 a = (u64)(unsigned long long)slot;
	u32 ins[6];
	unsigned long old;
	if (!p) return 0;
	*slot = target;
	ins[0] = 0xd2800011u | (u32)((a & 0xffffu) << 5);
	ins[1] = 0xf2a00011u | (u32)(((a >> 16) & 0xffffu) << 5);
	ins[2] = 0xf2c00011u | (u32)(((a >> 32) & 0xffffu) << 5);
	ins[3] = 0xf2e00011u | (u32)(((a >> 48) & 0xffffu) << 5);
	ins[4] = 0xf9400230u;
	ins[5] = 0xd61f0200u;
	__builtin_memcpy(p, ins, sizeof ins);
	VirtualProtect(p, 4096, PAGE_EXECUTE_READ, &old);
	FlushInstructionCache(GetCurrentProcess(), p, 24);
	return p;
}
int main(void){
	static void *si, *sf;
	int (*disp)(int) = (int (*)(int))mkslot(&si, (void *)target1);
	double (*fd)(double, double) = (double (*)(double, double))mkslot(&sf, (void *)fmul);
	int r1, r2, fr;
	if (!disp || !fd){ printf("alloc-fail\n"); return 2; }
	r1 = disp(5);            /* target1(5) = 116 */
	si = (void *)target2;    /* pointer-swap the dispatch slot */
	r2 = disp(5);            /* target2(5) = 227 */
	fr = (int)fd(3.0, 4.0);  /* fmul(3,4) = 15, d0 return through the slot */
	printf("r1=%d r2=%d fr=%d %s\n", r1, r2, fr,
	       (r1 == 116 && r2 == 227 && fr == 15) ? "OK" : "FAIL");
	return (r1 == 116 && r2 == 227 && fr == 15) ? 0 : 1;
}
EOF

echo "== host: mcc-arm64-win32 -> arm64 PE (hello + jit-dispatch) =="
"$MCC" -B"$MCCDIR" -o "$WORK_ABS/hello.exe" "$WORK_ABS/hello.c"
"$MCC" -B"$MCCDIR" -o "$WORK_ABS/jitdisp.exe" "$WORK_ABS/jitdisp.c"

echo "== docker linux/arm64: run both under wine-arm64 =="
# Git-Bash on Windows rewrites the container-side `/w` of `-v`/`-w`; disable its
# POSIX->Win path munging for just this command (a no-op on real POSIX hosts).
dg_docker run --rm --platform linux/arm64 -v "$WORK_ABS":/w -w /w "$IMAGE" sh -c '
	export WINEPREFIX=/tmp/wp
	wineboot -i >/dev/null 2>&1 || true
	fail=0
	out=$(wine hello.exe 2>/dev/null | tr -d "\r"); rc=$?
	if [ "$out" = "hello-arm64-pe" ] && [ $rc -eq 0 ]; then
		echo "PASS  hello arm64-PE runs ($out, rc=0)"
	else
		echo "FAIL  hello arm64-PE (out=$out rc=$rc)"; fail=1
	fi
	out=$(wine jitdisp.exe 2>/dev/null); rc=$?
	if [ $rc -eq 0 ]; then
		echo "PASS  jit-dispatch frameless-leaf slot+swap ($out)"
	else
		echo "FAIL  jit-dispatch ($out rc=$rc)"; fail=1
	fi
	exit $fail
'
