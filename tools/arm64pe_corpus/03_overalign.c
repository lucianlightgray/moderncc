/* Over-aligned types: the Tier-2 "over-align" work item. Stack and static
   objects with alignment > natural. The codegen (sp rounding, aligned stores)
   must match ELF-vs-PE; only the addressing of the static over-aligned global
   may diverge (benign GOT-vs-direct). Uses long long (width-stable) so the only
   expected diffs are addressing, not int-width. */

struct __attribute__((aligned(64))) Cacheline { long long v[8]; };

_Alignas(128) static long long g_over[16];

long long stack_over(long long x) {
    _Alignas(64) long long buf[8];
    for (int i = 0; i < 8; i++) buf[i] = x + i;
    return buf[0] + buf[7];
}

long long touch_static(int i) { g_over[i & 15] += 1; return g_over[0]; }

long long ret_cacheline_first(struct Cacheline *c) { return c->v[0] + c->v[7]; }
