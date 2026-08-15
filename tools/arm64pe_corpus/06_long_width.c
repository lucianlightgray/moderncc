// arm64pe-diff: datamodel-divergent
// `long` is 32-bit on arm64-PE (LLP64) and 64-bit on arm64-ELF (LP64), so the
// codegen legitimately differs (sign/zero-extension, operand widths, .text
// size). The LP64 ELF object is NOT a byte-oracle for the LLP64 PE object here;
// this subject exists to prove mcc honours each target's data model, and the
// harness validates only section structure for it (see tools/arm64pe_diff.py
// _datamodel_divergent). T-win-50007.
long shl2(long x) { return x << 2; }
long widen(int x) { return (long)x + 1; }
unsigned long masks(unsigned long a, unsigned long b) { return (a & b) | (a ^ b); }
