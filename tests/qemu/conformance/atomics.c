/* Self-checking C11 atomics conformance: in-language read-modify-write on
   _Atomic scalars/pointers (direct fetch helpers + compare-exchange loop +
   float), and the <stdatomic.h> builtins. All operands are <= a machine word
   so this is lock-free (no -latomic). Exercises codegen that is alignment- and
   ABI-sensitive (e.g. 8-byte atomics need 8-byte-aligned locals on 32-bit ARM,
   where LDREXD traps otherwise). Endianness-independent; exits 0 on success. */
#include <stdatomic.h>

int main(void)
{
    /* integer, direct fetch-op helpers (+ - & | ^, ++/--) */
    _Atomic int i = 7;
    int o = i++; if (o != 7 || i != 8) return 1;
    i += 5; if (i != 13) return 2;
    --i;     if (i != 12) return 3;
    i &= 12; if (i != 12) return 4;
    i |= 3;  if (i != 15) return 5;
    i ^= 1;  if (i != 14) return 6;

    /* integer, compare-exchange retry loop (* / % << >>) */
    i *= 2;  if (i != 28) return 7;
    i %= 10; if (i != 8)  return 8;
    i <<= 3; if (i != 64) return 9;
    i >>= 2; if (i != 16) return 10;

    /* 8-byte integer atomics (CAS loop) -- alignment-sensitive on 32-bit ARM */
    _Atomic long long ll = 3;
    ll *= 1000000000LL; if (ll != 3000000000LL) return 11;
    ll += 1;            if (ll != 3000000001LL) return 12;

    /* plain store AND in-language read of a wider-than-word atomic must be
       indivisible (on 32-bit each is a single __atomic_store_8/__atomic_load_8,
       not two word moves). */
    ll = 0x1122334455667788LL;
    long long ll_read = ll;                 /* in-language atomic read */
    if (ll_read != 0x1122334455667788LL) return 22;
    if ((ll >> 32) != 0x11223344LL) return 23;   /* read in an expression */

    /* float / double atomics (CAS loop, value passed as int bits) */
    _Atomic double d = 2.0;
    d *= 2.5; if (d != 5.0) return 13;
    d += 1.0; if (d != 6.0) return 14;
    ++d;      if (d != 7.0) return 15;
    _Atomic float f = 10.0f;
    f -= 3.0f; if (f != 7.0f) return 16;

    /* atomic pointer arithmetic (scaled by pointee size) */
    static int arr[8];
    _Atomic(int *) p = arr;
    p++; p += 3; p -= 1;
    if (p - arr != 3) return 17;

    /* <stdatomic.h> generic functions */
    atomic_int a = 100;
    if (atomic_fetch_add(&a, 5) != 100 || a != 105) return 18;
    if (atomic_exchange(&a, 200) != 105 || a != 200) return 19;
    int expected = 200;
    if (!atomic_compare_exchange_strong(&a, &expected, 201) || a != 201) return 20;
    atomic_store(&a, 42);
    if (atomic_load(&a) != 42) return 21;

    /* atomic_fetch_* across every C11 integer width (8/16/32/64-bit) -- C11
       has no integer type wider than long long, so this is the complete
       integer-atomic fetch-op surface. */
    atomic_uchar  c8  = 0xF0;
    atomic_ushort c16 = 0xFF00;
    atomic_uint   c32 = 0xFFFF0000u;
    atomic_ullong c64 = 0xFFFFFFFF00000000ULL;
    if (atomic_fetch_add(&c8,  1)    != 0xF0   || (unsigned char)c8   != 0xF1)        return 23;
    if (atomic_fetch_or (&c16, 0xFF) != 0xFF00 || (unsigned short)c16 != 0xFFFF)      return 24;
    if (atomic_fetch_and(&c32, 0xFFFFu) != 0xFFFF0000u || (unsigned)c32 != 0)         return 25;
    if (atomic_fetch_xor(&c64, ~0ULL) != 0xFFFFFFFF00000000ULL
        || (unsigned long long)c64 != 0x00000000FFFFFFFFULL)                          return 26;
    if (atomic_fetch_sub(&c64, 1ULL) != 0x00000000FFFFFFFFULL
        || (unsigned long long)c64 != 0x00000000FFFFFFFEULL)                          return 27;

    return 0;
}
