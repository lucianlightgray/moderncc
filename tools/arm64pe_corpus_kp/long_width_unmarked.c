/* Known-positive floor for tools/arm64pe_diff.py (T-lin-10047).  These are the
 * data-model-divergent `long`-width shapes, but this file deliberately OMITS
 * the datamodel-divergent opt-out marker, so the harness must NOT treat the
 * LLP64 (arm64-PE) vs LP64 (arm64-ELF) width divergence as benign -- it must
 * report it SUSPICIOUS.  If this file ever diffs clean, the harness has gone
 * blind to a divergence it must catch.  (The marker string is intentionally
 * absent from every line here.) */
long shl2(long x) { return x << 2; }
long widen(int x) { return (long)x + 1; }
unsigned long masks(unsigned long a, unsigned long b) { return (a & b) | (a ^ b); }
