#!/bin/sh
# A program built with --embed-jit must compute the same answers whether the
# runtime hot-swap installs new code or not.  MCC_JIT=0 keeps the AOT body, so
# every case here is its own control: the two runs differ only in whether the
# JIT re-emitted the function.
#
# This exists because the runtime re-emit ran with every ALWAYS-class replay
# flag off -- ast_configure() is called from mccgen_compile(), which the JIT's
# recompile path never reaches -- and silently produced wrong code for any
# binary operator over two comparison results.  At compile time the same defect
# is invisible: the faithfulness gate compares replay against the parser bytes
# and falls back.  At runtime nothing compares, so the wrong code was installed.
# tests/diff/full_language.c carries the shape in isid(), but no cell built it
# with --embed-jit.
set -e
MCC="$1"
BD="$2"
INC="$3"
WORK="$4"

SRC="$WORK/jit_replay_parity_prog.c"
EXE="$WORK/jit_replay_parity_prog"
fails=0

case_run() {
	desc="$1"
	body="$2"
	want="$3"
	cat > "$SRC" <<CEOF
int printf(const char *, ...);
int f(int c) { $body }
int main(void) {
	printf("%d %d %d %d\n", f(3), f(20), f(100), f(200));
	return 0;
}
CEOF
	"$MCC" -B"$BD" -I"$INC" -w --embed-jit "$SRC" -o "$EXE" -lm || {
		echo "FAIL: $desc: compile failed"
		fails=$((fails + 1))
		return
	}
	on=$(env -u LD_LIBRARY_PATH MCC_JIT=1 "$EXE" 2>/dev/null) || {
		echo "FAIL: $desc: run with JIT on failed"
		fails=$((fails + 1))
		return
	}
	off=$(env -u LD_LIBRARY_PATH MCC_JIT=0 "$EXE" 2>/dev/null) || {
		echo "FAIL: $desc: run with JIT off failed"
		fails=$((fails + 1))
		return
	}
	if [ "$on" != "$off" ]; then
		echo "FAIL: $desc: JIT changed the answer"
		echo "        MCC_JIT=0 (AOT): [$off]"
		echo "        MCC_JIT=1 (JIT): [$on]"
		fails=$((fails + 1))
		return
	fi
	if [ "$on" != "$want" ]; then
		echo "FAIL: $desc: both arms agree but are wrong"
		echo "        want: [$want]"
		echo "        got:  [$on]"
		fails=$((fails + 1))
		return
	fi
	echo "ok: $desc"
}

# A binary operator whose BOTH operands are comparisons is the shape that broke:
# the first comparison's flags must be materialised before the second issues.
case_run "and of two compares"    "return (c > 5) & (c < 100);"             "0 1 0 0"
case_run "or of two compares"     "return (c > 5) | (c < 100);"             "1 1 1 1"
case_run "xor of two compares"    "return (c > 5) ^ (c < 100);"             "1 0 1 1"
case_run "add of two compares"    "return (c > 5) + (c < 100);"             "1 2 1 1"
case_run "mul of two compares"    "return (c > 5) * (c < 100);"             "0 1 0 0"
case_run "three chained compares" "return (c > 5) & (c < 100) & (c != 7);"  "0 1 0 0"
# isid() from tests/diff/full_language.c -- the in-tree program that caught it.
case_run "full_language isid" \
	"return (c >= 'a' & c <= 'z') | (c >= 'A' & c <= 'Z') | c == '_';" "0 0 1 0"
# Controls: these were correct before the fix and must stay correct.
case_run "compare against constant" "return (c > 5) & 1;"                   "0 1 1 1"
case_run "compare via a local"      "int t = c > 5; return t & (c < 100);"  "0 1 0 0"
case_run "logical and/or"           "return (c > 5) && (c < 100);"          "0 1 0 0"

if [ "$fails" -ne 0 ]; then
	echo "FAIL: $fails case(s) diverged between the JIT and the AOT body"
	exit 1
fi
echo "PASS: every case agrees with the JIT on and off"
exit 0
