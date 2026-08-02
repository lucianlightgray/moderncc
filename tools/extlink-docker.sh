#!/bin/sh

set -eu
. "$(dirname "$0")/dockergate.sh"

MCC="${1:-}"
PLAT="${2:-}"
WORK="${3:-./w-extlink}"
CROSS="${4:-}"

case "$PLAT" in
	linux/amd64) IMAGE="${MCC_EXTLINK_AMD64_IMAGE:-debian:bookworm-slim}" ;;
	linux/arm64) IMAGE="${MCC_EXTLINK_ARM64_IMAGE:-arm64v8/debian:bookworm-slim}" ;;
	*) echo "SKIP: unsupported platform '${PLAT:-<unset>}'"; exit 77 ;;
esac

dg_need_bin "$MCC" "mcc"
dg_need_docker
dg_need_platform "$PLAT" "$IMAGE"

rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS=$(cd "$WORK" && pwd)

cat > "$WORK_ABS/m.c" <<'EOF'
unsigned ud(unsigned x){ return x / 7u; }
int sd(int x){ return x / 7; }
long long ll(long long x){ return x / 7; }
unsigned long long ull(unsigned long long x){ return x / 7ull; }
int sel(int a, int b){ return a < b ? a : b; }
int main(void){
	unsigned bad = 0; long i;
	for (i = -100000; i <= 100000; i += 37) {
		if (ud((unsigned)i) != (unsigned)i / 7u) bad++;
		if (sd((int)i) != (int)i / 7) bad++;
		if (ll(i) != i / 7) bad++;
		if (ull((unsigned long long)i) != (unsigned long long)i / 7ull) bad++;
		if (sel((int)i, (int)i + 3) != ((int)i < (int)i + 3 ? (int)i : (int)i + 3)) bad++;
	}
	return (int)bad;
}
EOF

echo "== host: mcc -> ELF objects (-O0, -O2, -gdwarf-4) =="
"$MCC" -O0 -c "$WORK_ABS/m.c" -o "$WORK_ABS/m0.o"
"$MCC" -O2 -c "$WORK_ABS/m.c" -o "$WORK_ABS/m2.o"
"$MCC" -gdwarf-4 -O0 -c "$WORK_ABS/m.c" -o "$WORK_ABS/mg.o"

if [ -n "$CROSS" ]; then
	echo "== docker $PLAT ($CROSS cross): link mcc objects with GNU ld, check DWARF (no exec) =="
else
	echo "== docker $PLAT: link mcc objects with GNU ld (gcc), run, check DWARF =="
fi
dg_docker run --rm --platform "$PLAT" -e CROSS="$CROSS" -v "$WORK_ABS":/w -w /w "$IMAGE" sh -c '
	GCC="${CROSS}gcc"; READELF="${CROSS}readelf"
	if [ -n "$CROSS" ]; then t=$(echo "$CROSS" | sed "s/-$//"); PKG="gcc-$t binutils-$t"; else PKG="gcc binutils"; fi
	command -v "$GCC" >/dev/null 2>&1 || { apt-get update >/dev/null 2>&1; apt-get install -y $PKG >/dev/null 2>&1; }
	fail=0
	for o in 0 2; do
		if ! "$GCC" m${o}.o -o m${o} 2>gcc_err; then
			echo "FAIL  -O$o external link:"; sed "s/^/    /" gcc_err; fail=1; continue
		fi
		if [ -n "$CROSS" ]; then echo "OK    -O$o external link (no exec: cross target)"; continue; fi
		rc=0; ./m${o} || rc=$?
		if [ "$rc" = 0 ]; then echo "OK    -O$o link+run (exit 0)"; else echo "FAIL  -O$o run exit=$rc (should be 0)"; fail=1; fi
	done
	if ! "$GCC" -g mg.o -o mg 2>gcc_err; then
		echo "FAIL  -gdwarf external link:"; sed "s/^/    /" gcc_err; fail=1
	else
		n=$("$READELF" --debug-dump=info mg 2>/dev/null | awk "/DW_TAG_subprogram/{p=1} p&&/DW_AT_low_pc/{print \$NF; p=0}" | sort -u | wc -l)
		if [ "$n" -ge 5 ]; then echo "OK    -gdwarf $n distinct subprogram low_pc"; else echo "FAIL  -gdwarf only $n distinct low_pc (collapsed onto .text+0)"; fail=1; fi
	fi
	exit $fail
'
