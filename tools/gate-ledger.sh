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

# Dev-gated rows are refused by the compiler unless MCC_DEV=1 is set -- refused,
# deliberately, rather than ignored, so the flag cannot be mistaken for one that
# does not exist. That means `-f<devgate>` compiles NOTHING, and this ledger used
# to score the resulting whole-corpus compile failure as the loudest FIRES on the
# board: all twelve reported an identical astfn=1319/asttu=292/objtu=299, which
# is not twelve optimizations coincidentally moving the same 1319 functions, it
# is the corpus failing to build twelve times. They are measured below in a
# second pass with MCC_DEV=1, where the flag is actually accepted.
grep -oE 'MCC_OPT_ROW\([A-Z0-9_]+, *"[a-z0-9-]+", *MCC_OPTD_DEV' src/mccopt.h |
	sed 's/.*"\([a-z0-9-]*\)".*/\1/' | sort -u > "$WORK/gates.dev"
comm -23 "$WORK/gates.txt" "$WORK/gates.dev" > "$WORK/gates.plain"

cat > "$WORK/cell.sh" <<'CELL'
#!/usr/bin/env bash
set -u
REPO=$1; MCC=$2; OPT=$3; WORK=$4; G=$5; DEV=$6; shift 6
T="$WORK/cells/$G"
rm -rf "$T"; mkdir -p "$T"
[ "$DEV" = 1 ] && export MCC_DEV=1
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
# A polarity that built NOTHING while the other built something did not measure
# an optimization, it measured a rejected flag. Say so instead of reporting the
# whole corpus as a difference -- comparing output a compile never produced is
# the single most repeated way this tree has fooled itself.
NTU=$(wc -l < "$WORK/corpus.txt" | tr -d ' ')
N0=$(grep -c ' NOOBJ$' "$T/o0" || true)
N1=$(grep -c ' NOOBJ$' "$T/o1" || true)
if [ "$N1" -ge "$NTU" ] && [ "$N0" -lt "$NTU" ]; then
	printf -- '-f%s rejected: %s/%s TUs failed to build\n' "$G" "$N1" "$NTU" > "$T/refused"
elif [ "$N0" -ge "$NTU" ] && [ "$N1" -lt "$NTU" ]; then
	printf -- '-fno-%s rejected: %s/%s TUs failed to build\n' "$G" "$N0" "$NTU" > "$T/refused"
fi

comm -3 "$T/h0" "$T/h1" | sed 's/^	//' | sort -u > "$T/dh"
diff "$T/o0" "$T/o1" | grep '^<' | awk '{print $2}' | sort -u > "$T/do"
CELL
chmod +x "$WORK/cell.sh"

echo "gate-ledger: $NG gate(s) ($(wc -l < "$WORK/gates.dev" | tr -d ' ') dev-gated, measured under MCC_DEV=1),"
echo "             corpus $(wc -l < "$WORK/corpus.txt" | tr -d ' ') file(s) at $OPT"

# One control cell per pass. The control is a name the compiler never reads, so
# whatever it "changes" is run-to-run noise and is subtracted from every gate in
# the same pass. MCC_DEV=1 changes the compiler's own defaults, so the dev pass
# needs its own control rather than borrowing the plain one's.
for pass in plain dev; do
	[ -s "$WORK/gates.$pass" ] || continue
	[ "$pass" = dev ] && D=1 || D=0
	"$WORK/cell.sh" "$REPO" "$MCC" "$OPT" "$WORK" "gateledger-control-$pass" "$D" $MCCFLAGS
	C="$WORK/cells/gateledger-control-$pass"
	awk '{print $1" "$2}' "$C/dh" | sort -u > "$WORK/noise.fn.$pass"
	sort -u "$C/do" > "$WORK/noise.tu.$pass"
	echo "gate-ledger: $pass control cell is not silent: $(wc -l < "$WORK/noise.fn.$pass" | tr -d ' ') function(s)" \
		"and $(wc -l < "$WORK/noise.tu.$pass" | tr -d ' ') object(s) differ run-to-run; both are subtracted"
	xargs -P "$JOBS" -I@ "$WORK/cell.sh" "$REPO" "$MCC" "$OPT" "$WORK" @ "$D" $MCCFLAGS < "$WORK/gates.$pass"
done

: > "$WORK/ledger.txt"
while read -r g; do
	T="$WORK/cells/$g"
	if grep -qx "$g" "$WORK/gates.dev"; then pass=dev; else pass=plain; fi
	[ -f "$T/dh" ] || { printf 'MISSING\t%s\n' "$g" >> "$WORK/ledger.txt"; continue; }
	if [ -f "$T/refused" ]; then
		printf 'REFUSED\t%s\t%s\n' "$g" "$(cat "$T/refused")" >> "$WORK/ledger.txt"
		continue
	fi
	awk '{print $1" "$2}' "$T/dh" | sort -u | comm -23 - "$WORK/noise.fn.$pass" > "$T/dh.net"
	sort -u "$T/do" | comm -23 - "$WORK/noise.tu.$pass" > "$T/do.net"
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
echo "== REFUSED: the compiler rejected the flag, so the gate was not measured"
grep '^REFUSED' "$WORK/ledger.txt" | cut -f2- | sed 's/^/  /' || true
echo
printf 'gate-ledger %s: %s change the AST, %s change only the object, %s never fire, %s refused, of %s gate(s)\n' \
	"$OPT" \
	"$(grep -c '^FIRES' "$WORK/ledger.txt" || true)" \
	"$(grep -c '^OBJONLY' "$WORK/ledger.txt" || true)" \
	"$(grep -c '^NEVER' "$WORK/ledger.txt" || true)" \
	"$(grep -c '^REFUSED' "$WORK/ledger.txt" || true)" \
	"$NG"
