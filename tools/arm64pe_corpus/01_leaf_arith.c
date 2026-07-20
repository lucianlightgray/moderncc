/* Baseline: leaf integer arithmetic on WIDTH-STABLE types (int / long long are
   the same size under LP64 and LLP64). No externs, no relocs in .text, so the
   .text here MUST be byte-identical ELF-vs-PE. Any divergence is a real bug.
   NB: `long` is deliberately avoided here — it is 8 bytes (LP64) on arm64-Linux
   but 4 bytes (LLP64) on arm64-Windows, an expected ABI divergence covered by
   06_long_width.c. */
int add3(int a, int b) { return a + b * 3; }
long long mix(long long a, long long b, long long c) {
    return (a << 2) + (b ^ c) - (a & b);
}
unsigned udiv(unsigned x, unsigned y) { return x / y + x % y; }
