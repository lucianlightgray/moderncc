#!/bin/sh
# T-mac-30023: the object/archive readers must REJECT malformed untrusted input
# with a normal error exit, never crash. Guards two memory-safety fixes:
#   - archive symbol-table nsyms OOB read (src/objfmt/mccelf.c, mcc_load_alacarte)
#   - COFF relocation VirtualAddress OOB write (src/objfmt/mccpe.c, coff reloc loop)
# A crash (signal / rc>=128) is a FAIL; a clean diagnostic exit is a PASS.
set -u

MCC="$1"
CROSSDIR="$2"
WORK="$3"

mkdir -p "$WORK" 2>/dev/null || exit 2
cd "$WORK" || exit 2
command -v ar >/dev/null 2>&1 || exit 77
command -v python3 >/dev/null 2>&1 || exit 77
[ -x "$MCC" ] || exit 77

crashed() { [ "$1" -ge 128 ]; }

# ---- 1. archive nsyms OOB read (native) ----
cat > a.c <<'EOF'
int alpha(void) { return 1; }
int beta(void) { return 2; }
EOF
"$MCC" -c a.c -o a.o || exit 77
rm -f good.a
ar rcs good.a a.o || exit 77
cat > m.c <<'EOF'
int main(void) { return 0; }
EOF
arch_kind=$(python3 - <<'PY'
import struct
d = bytearray(open("good.a", "rb").read())
assert d[:8] == b"!<arch>\n"
name = bytes(d[8:8 + 16]).rstrip()
# ar rcs writes the GNU "/" symbol-table member first: 60-byte header then a
# big-endian uint32 symbol count. Inflate it so the derived name-region
# pointer (data + 4 + nsyms*4) lands ~1GB past the tiny member allocation
# (deterministically unmapped) without overflowing the int stride math, so a
# pre-fix build faults reproducibly rather than reading adjacent heap.
if name[:1] == b"/" and name[:2] != b"//":
	struct.pack_into(">I", d, 8 + 60, 0x10000000)
	open("bad.a", "wb").write(d)
	print("gnu")
else:
	print("bsd")
PY
) || exit 77
if [ "$arch_kind" = gnu ]; then
	out=$("$MCC" m.c bad.a -o out_arch 2>&1)
	rc=$?
	if crashed "$rc"; then
		echo "FAIL: archive nsyms — mcc crashed (rc=$rc) on malformed archive"
		echo "$out"
		exit 1
	fi
	if [ "$rc" -eq 0 ]; then
		echo "FAIL: archive nsyms — mcc accepted a malformed archive (rc=0)"
		exit 1
	fi
	case "$out" in
		*"invalid archive"*) : ;;
		*) echo "FAIL: archive nsyms — expected 'invalid archive', got: $out"; exit 1 ;;
	esac
else
	echo "SKIP: archive nsyms OOB targets the GNU \"/\" symbol-table member; host ar produced a BSD/mach-o archive (member0 \"#1/..\" __.SYMDEF), which does not exercise mcc_load_alacarte"
fi
# the well-formed archive must still be accepted (no false reject)
"$MCC" m.c good.a -o out_ok || { echo "FAIL: valid archive rejected"; exit 1; }

# ---- 2. COFF reloc VirtualAddress OOB write (needs the x86_64-win32 cross mcc) ----
WMCC="$CROSSDIR/mcc-x86_64-win32"
if [ -x "$WMCC" ]; then
	cat > c.c <<'EOF'
extern int g;
int *p = &g;
EOF
	if "$WMCC" -c c.c -o c.o 2>/dev/null; then
		python3 - <<'PY' || exit 77
import struct
d = bytearray(open("c.o", "rb").read())
nsec = struct.unpack_from("<H", d, 2)[0]
off = 20
for _ in range(nsec):
	ptrrel = struct.unpack_from("<I", d, off + 24)[0]
	nrel = struct.unpack_from("<H", d, off + 32)[0]
	if nrel > 0:
		struct.pack_into("<I", d, ptrrel, 0x7fffffff)  # reloc VirtualAddress
		break
	off += 40
open("c_bad.o", "wb").write(d)
PY
		out=$("$WMCC" -r c_bad.o -o cmerged.o 2>&1)
		rc=$?
		if crashed "$rc"; then
			echo "FAIL: COFF reloc — cross mcc crashed (rc=$rc) on malformed object"
			echo "$out"
			exit 1
		fi
		if [ "$rc" -eq 0 ]; then
			echo "FAIL: COFF reloc — cross mcc accepted an out-of-range reloc (rc=0)"
			exit 1
		fi
		case "$out" in
			*"outside its section"*) : ;;
			*) echo "FAIL: COFF reloc — expected 'outside its section', got: $out"; exit 1 ;;
		esac
		"$WMCC" -r c.o -o cok.o || { echo "FAIL: valid COFF object rejected"; exit 1; }
	fi
fi

# ---- 3. ELF section-header index OOB (T-mac-30091, native ELF only) ----
python3 - <<'PY' || exit 77
import struct
d = bytearray(open("a.o", "rb").read())
# only meaningful for a 64-bit little-endian ELF object (native lin-x64 build)
if d[:4] == b"\x7fELF" and d[4] == 2 and d[5] == 1:
	# ELF64 Ehdr: e_shnum @60, e_shstrndx @62 (both uint16 LE)
	shnum = struct.unpack_from("<H", d, 60)[0]
	struct.pack_into("<H", d, 62, 0xffff)  # e_shstrndx far past e_shnum
	open("bad_elf.o", "wb").write(d)
PY
if [ -f bad_elf.o ]; then
	out=$("$MCC" m.c bad_elf.o -o out_elf 2>&1)
	rc=$?
	if crashed "$rc"; then
		echo "FAIL: ELF shndx — mcc crashed (rc=$rc) on malformed object"
		echo "$out"
		exit 1
	fi
	if [ "$rc" -eq 0 ]; then
		echo "FAIL: ELF shndx — mcc accepted a malformed object (rc=0)"
		exit 1
	fi
	case "$out" in
		*"invalid object file"*) : ;;
		*) echo "FAIL: ELF shndx — expected 'invalid object file', got: $out"; exit 1 ;;
	esac
	# the pristine object must still link (no false reject)
	"$MCC" m.c a.o -o out_elf_ok || { echo "FAIL: valid ELF object rejected"; exit 1; }
	rm -f bad_elf.o
fi

echo "PASS: malformed object/archive inputs rejected cleanly"
exit 0
