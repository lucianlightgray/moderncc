/* -S round-trip fixture: exercises a broad slice of the integer instruction
 * subset the x86_64 backend emits (arithmetic, shifts, comparisons, branches,
 * loops, recursion, switch, structs, globals, string constants, pointer
 * indexing, function pointers) so the disassembler is well covered.  Kept
 * integer/pointer only so mcc's own integrated assembler can re-assemble the
 * -S output (it has no SSE mnemonics).  The program prints a deterministic
 * line; the driver checks the -S round-tripped build prints the same thing.
 */
extern int printf(const char *, ...);

struct pt { int x, y; };
static int g_counter = 41;
static const char *msg = "rt";

static int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }

static long sumarr(const int *a, int n)
{
    long s = 0;
    for (int i = 0; i < n; i++)
        s += a[i] << 1;
    return s;
}

static int classify(int x)
{
    switch (x) {
    case 0:  return 100;
    case 3:  return 300;
    case 9:  return 900;
    default: return x * 2 - 1;
    }
}

static unsigned bits(unsigned v) { return (v & 0xff) | (v >> 4) ^ (v << 3); }

static int add1(int x) { return x + 1; }
static int neg(int x)  { return -x; }

int main(void)
{
    int a[6] = { 3, 1, 4, 1, 5, 9 };
    struct pt p = { 7, 35 };
    int (*fp[2])(int) = { add1, neg };
    int acc = g_counter + p.x + p.y;

    for (int i = 0; i < 2; i++)
        acc = fp[i](acc);

    printf("%s %d %ld %d %d %u %d\n",
           msg, fact(6), sumarr(a, 6), classify(9), classify(5),
           bits(0x1234u), acc);
    return 0;
}
