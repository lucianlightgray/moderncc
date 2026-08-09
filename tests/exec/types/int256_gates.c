#if defined test_to_double
double f(__int256 x) { return (double)x; }

#elif defined test_from_double
__int256 f(double d) { return (__int256)d; }

#elif defined test_to_float
float f(__int256 x) { return (float)x; }

#elif defined test_to_long_double
long double f(__int256 x) { return (long double)x; }

#elif defined test_switch
int f(__int256 x) {
	switch (x) {
	case 1:
		return 2;
	}
	return 0;
}

#elif defined test_bitfield
struct S {
	__int256 a : 3;
};

#elif defined test_vector
typedef __int256 v __attribute__((vector_size(64)));
v g;

#elif defined test_long_int256
long __int256 x;

#elif defined test_int256_int
__int256 int x;

#elif defined test_complex
_Complex __int256 x;

#elif defined test_pointer_cast
__int256 *p;
int f(void) { return (int)(__int256)p; }

#elif defined test_div_zero_const
__int256 g = (__int256)1 / 0;

#elif defined test_mod_zero_const
__int256 g = (__int256)1 % 0;

#elif defined test_nonconst_init
__int256 a;
__int256 g = a + 1;

#elif defined test_struct_assign
struct S {
	int a;
};
int f(struct S s) {
	__int256 v;
	v = s;
	return (int)v;
}

#elif defined test_signed_ok
#include <stdio.h>
int main(void) {
	signed __int256 a = -1;
	unsigned __int256 b = 3;
	printf("%d %d\n", (int)a, (int)b);
	return 0;
}

#endif
