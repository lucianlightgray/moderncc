/* Regression: an _Atomic-qualified type whose object size is a power of two in
 * {1,2,4,8,16} must have its alignment promoted UP to that size, matching C11
 * and gcc-16/clang. mcc kept the natural (member) alignment, so
 * `_Atomic struct{long,long}` reported _Alignof 8 instead of 16, and an
 * _Atomic member placed in an enclosing struct sat at the wrong offset,
 * corrupting sizeof/_Alignof/offsetof and the cross-TU ABI (16-byte atomics
 * need 16-byte alignment for CASP/LDXP). The fix bumps *a in type_size when
 * VT_ATOMIC_BIT is set and the size is a lock-free power of two (T-mac-30242).
 * Exit 0 only when every case matches the oracle values gcc-16/clang produce. */

typedef _Atomic struct { char a, b; }   A_cc;   /* size 2  -> align 2  */
typedef _Atomic struct { char x[8]; }    A_c8;   /* size 8  -> align 8  */
typedef _Atomic struct { int a, b; }     A_ii;   /* size 8  -> align 8  */
typedef _Atomic struct { long a, b; }    A_ll;   /* size 16 -> align 16 */

struct nest { char c; A_ll s; };                 /* size 32, off(s)=16  */

/* A non-power-of-two _Atomic aggregate must NOT be over-aligned: gcc keeps its
 * natural alignment (it is routed through libatomic locks, not lock-free). */
typedef _Atomic struct { char x[3]; }    A_c3;   /* size 3  -> align 1  */

int main(void) {
	if (sizeof(A_cc) != 2  || _Alignof(A_cc) != 2)  return 1;
	if (sizeof(A_c8) != 8  || _Alignof(A_c8) != 8)  return 2;
	if (sizeof(A_ii) != 8  || _Alignof(A_ii) != 8)  return 3;
	if (sizeof(A_ll) != 16 || _Alignof(A_ll) != 16) return 4;
	if (sizeof(struct nest) != 32 || _Alignof(struct nest) != 16) return 5;
	if (__builtin_offsetof(struct nest, s) != 16)   return 6;
	if (sizeof(A_c3) != 3  || _Alignof(A_c3) != 1)  return 7;
	return 0;
}
