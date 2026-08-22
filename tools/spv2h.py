#!/usr/bin/env python3
# spv2h.py -- embed a SPIR-V binary as a 4-byte-aligned C uint32 array header.
# Usage: spv2h.py <in.spv> <symbol> <out.h>
# The array is little-endian 32-bit words so it can be passed straight to
# vkCreateShaderModule as pCode (uint32_t*), codeSize = sizeof(array).
import sys, struct

def main():
    if len(sys.argv) != 4:
        sys.stderr.write("usage: spv2h.py <in.spv> <symbol> <out.h>\n")
        return 2
    spv, sym, out = sys.argv[1], sys.argv[2], sys.argv[3]
    data = open(spv, "rb").read()
    if len(data) % 4:
        sys.stderr.write("spv2h: %s is not 4-byte aligned\n" % spv)
        return 1
    words = struct.unpack("<%dI" % (len(data) // 4), data)
    guard = "_" + sym.upper() + "_H"
    L = []
    L.append("#ifndef " + guard)
    L.append("#define " + guard)
    L.append("/* SPIR-V embedded by tools/spv2h.py -- GENERATED, do not hand-edit. */")
    L.append("static const unsigned int %s[] = {" % sym)
    for i in range(0, len(words), 6):
        L.append("\t" + " ".join("0x%08xu," % w for w in words[i:i + 6]))
    L.append("};")
    L.append("#endif")
    open(out, "w").write("\n".join(L) + "\n")
    return 0

if __name__ == "__main__":
    sys.exit(main())
