#!/usr/bin/env bash
set -eu

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"
MCC="${MCC:-$REPO/cmake-debug/mcc}"
OPT="${OPT:--O1}"
JOBS="${JOBS:-8}"
MCCFLAGS="${MCCFLAGS:-}"
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC (set MCC=)"; exit 77; }

WORK="${GATELEDGER_WORK:-$(mktemp -d)}"
[ -n "${GATELEDGER_WORK:-}" ] || trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/cells"

find tests/exec -name '*.c' | sort > "$WORK/corpus.txt"
echo tests/diff/full_language.c >> "$WORK/corpus.txt"

grep -oE 'MCC_OPT_ROW\([A-Z0-9_]+, *"[a-z0-9-]+"' src/mccopt.h |
	sed 's/.*"\([a-z0-9-]*\)"/\1/' | sort -u > "$WORK/gates.txt"
NG=$(wc -l < "$WORK/gates.txt" | tr -d ' ')
[ "$NG" -gt 0 ] || { echo "FAIL: no MCC_OPT_ROW names parsed from src/mccopt.h"; exit 1; }

cat > "$WORK/cell.sh" <<'CELL'
#!/usr/bin/env bash
set -u
REPO=$1; MCC=$2; OPT=$3; WORK=$4; G=$5; shift 5
T="$WORK/cells/$G"
rm -rf "$T"; mkdir -p "$T"
for v in 0 1; do
	if [ "$v" = 0 ]; then GF="-fno-$G"; else GF="-f$G"; fi
	: > "$T/h$v"; : > "$T/o$v"
	while read -r f; do
		rm -f "$T/hf" "$T/a.o"
		( cd "$REPO" && env MCC_AST_HASH_OUT="$T/hf" \
			"$MCC" -w "$OPT" "$GF" "$@" -c -o "$T/a.o" "$f" ) >/dev/null 2>&1
		if [ $? -eq 0 ] && [ -f "$T/a.o" ]; then
			echo "$f $(cksum < "$T/a.o")" >> "$T/o$v"
		else
			echo "$f NOOBJ" >> "$T/o$v"
		fi
		[ -f "$T/hf" ] && sed "s|^|$f |" "$T/hf" >> "$T/h$v"
	done < "$WORK/corpus.txt"
	sort -o "$T/h$v" "$T/h$v"
done
comm -3 "$T/h0" "$T/h1" | sed 's/^	//' | sort -u > "$T/dh"
diff "$T/o0" "$T/o1" | grep '^<' | awk '{print $2}' | sort -u > "$T/do"
CELL
chmod +x "$WORK/cell.sh"

"$WORK/cell.sh" "$REPO" "$MCC" "$OPT" "$WORK" gateledger-control $MCCFLAGS
awk '{print $1" "$2}' "$WORK/cells/gateledger-control/dh" | sort -u > "$WORK/noise.fn"
sort -u "$WORK/cells/gateledger-control/do" > "$WORK/noise.tu"
NF=$(wc -l < "$WORK/noise.fn" | tr -d ' ')
NT=$(wc -l < "$WORK/noise.tu" | tr -d ' ')
echo "gate-ledger: $NG gate(s), corpus $(wc -l < "$WORK/corpus.txt" | tr -d ' ') file(s) at $OPT"
echo "gate-ledger: control cell (a name the compiler never reads) is not silent:"
echo "             $NF function(s) and $NT object(s) differ run-to-run; both are subtracted"

xargs -P "$JOBS" -I@ "$WORK/cell.sh" "$REPO" "$MCC" "$OPT" "$WORK" @ $MCCFLAGS < "$WORK/gates.txt"

: > "$WORK/ledger.txt"
while read -r g; do
	T="$WORK/cells/$g"
	[ -f "$T/dh" ] || { printf 'MISSING\t%s\n' "$g" >> "$WORK/ledger.txt"; continue; }
	awk '{print $1" "$2}' "$T/dh" | sort -u | comm -23 - "$WORK/noise.fn" > "$T/dh.net"
	sort -u "$T/do" | comm -23 - "$WORK/noise.tu" > "$T/do.net"
	nfn=$(wc -l < "$T/dh.net" | tr -d ' ')
	ntu=$(awk '{print $1}' "$T/dh.net" | sort -u | wc -l | tr -d ' ')
	nob=$(wc -l < "$T/do.net" | tr -d ' ')
	if [ "$nfn" -eq 0 ] && [ "$nob" -eq 0 ]; then v=NEVER
	elif [ "$nfn" -eq 0 ]; then v=OBJONLY
	else v=FIRES; fi
	printf '%s\t%s\tastfn=%s\tasttu=%s\tobjtu=%s\n' "$v" "$g" "$nfn" "$ntu" "$nob" >> "$WORK/ledger.txt"
done < "$WORK/gates.txt"

echo
echo "== FIRES: changes the recorded AST (MCC_AST_HASH_OUT identity per function)"
grep '^FIRES' "$WORK/ledger.txt" | sort -t= -k2 -rn || true
echo
echo "== OBJONLY: recorded AST identical, emitted object differs (fires after the hash is taken)"
grep '^OBJONLY' "$WORK/ledger.txt" | sort -t= -k4 -rn || true
echo
echo "== NEVER: no AST change and no object change anywhere in the corpus"
grep '^NEVER' "$WORK/ledger.txt" | awk '{print "  "$2}' || true
echo
printf 'gate-ledger %s: %s change the AST, %s change only the object, %s never fire, of %s gate(s)\n' \
	"$OPT" \
	"$(grep -c '^FIRES' "$WORK/ledger.txt" || true)" \
	"$(grep -c '^OBJONLY' "$WORK/ledger.txt" || true)" \
	"$(grep -c '^NEVER' "$WORK/ledger.txt" || true)" \
	"$NG"
