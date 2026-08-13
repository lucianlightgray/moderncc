#!/bin/sh
# pe/dwarf-lines (W5): mcc's PE output carries usable DWARF line info.
# mcc -gdwarf-N on a PE target emits .debug_line that maps each function's
# entry address back to the right source file:line, consumable by the GNU
# tools (addr2line / gdb). This measures the debuggability mcc already has;
# the residual W5 gap is CodeView/PDB for Microsoft debuggers specifically.
#
# usage: pe-dwarf-lines.sh <mcc> <objdump> <addr2line> <src> <work>
set -u
MCC="$1"; OBJDUMP="$2"; ADDR2LINE="$3"; SRC="$4"; WORK="$5"

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then echo "pe-dwarf-lines: no mcc: $MCC" >&2; exit 77; fi
if [ ! -f "$OBJDUMP" ] || [ ! -f "$ADDR2LINE" ]; then
	echo "pe-dwarf-lines: need mingw objdump/addr2line (skip)" >&2; exit 77
fi

rm -rf "$WORK"; mkdir -p "$WORK"
base=$(basename "$SRC" .c)
fail=0

# each function and the source line its entry (prologue) maps to -- DWARF
# attributes the entry to the function's declaration line
set -- "square:1" "cube:6" "main:11"

for opt in -gdwarf-4 -gdwarf-2; do
	exe="$WORK/${base}${opt}.exe"
	if ! "$MCC" $opt "$SRC" -o "$exe" 2>"$WORK/b$opt.log"; then
		echo "pe-dwarf-lines: mcc $opt build failed" >&2; cat "$WORK/b$opt.log" >&2; fail=1; continue
	fi
	secs=$("$OBJDUMP" -h "$exe" 2>/dev/null | grep -oE '\.debug_line' | head -1)
	if [ "$secs" != ".debug_line" ]; then
		echo "pe-dwarf-lines: mcc $opt emitted no .debug_line section" >&2; fail=1; continue
	fi
	# .text virtual address (symbol values in `objdump -t` are relative to it)
	tvma=$("$OBJDUMP" -h "$exe" 2>/dev/null | awk '$2==".text"{print $4; exit}')
	for pair in "$@"; do
		fn=${pair%%:*}; want=${pair##*:}
		# PE `objdump -t` line: "[idx](sec N)(fl ..)(ty ..)(scl ..) (nx N) 0xADDR name"
		off=$("$OBJDUMP" -t "$exe" 2>/dev/null | grep -E " $fn\$" | head -1 | awk '{print $(NF-1)}')
		case "$off" in 0x*) off=${off#0x} ;; esac
		if [ -z "$off" ]; then echo "pe-dwarf-lines: $opt no symbol for $fn" >&2; fail=1; continue; fi
		addr=$(printf '0x%x' $(( 0x$tvma + 0x$off )))
		gotfn=$("$ADDR2LINE" -e "$exe" -f "$addr" 2>/dev/null | head -1)
		line=$("$ADDR2LINE" -e "$exe" "$addr" 2>/dev/null | sed -nE 's/.*:([0-9]+).*/\1/p' | head -1)
		if [ "$line" = "$want" ] && [ "$gotfn" = "$fn" ]; then
			echo "mcc $opt: $fn@$addr -> $gotfn:$line (ok)"
		else
			echo "pe-dwarf-lines: $opt $fn@$addr mapped to '$gotfn:$line', expected $fn:$want" >&2
			fail=1
		fi
	done
done

if [ "$fail" != "0" ]; then exit 1; fi
echo "pe-dwarf-lines: OK (mcc PE DWARF line table resolves every function via addr2line)"
exit 0
