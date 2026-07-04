#include <stdlib.h>

#ifdef _WIN32

__declspec(dllimport) void *__stdcall LoadLibraryA(const char *name);
__declspec(dllimport) void *__stdcall GetProcAddress(void *mod, const char *name);
typedef lldiv_t (*lldiv_fn)(long long, long long);
static lldiv_t rt_lldiv(long long n, long long d) {
	void *m = LoadLibraryA("msvcrt.dll");
	lldiv_fn fn = m ? (lldiv_fn)GetProcAddress(m, "lldiv") : (lldiv_fn)0;
	if (fn)
		return fn(n, d);
	lldiv_t r;
	r.quot = n / d;
	r.rem = n % d;
	return r;
}
#define lldiv(n, d) rt_lldiv((n), (d))
#endif

int main(void) {

	div_t d = div(17, 5);
	if (d.quot != 3 || d.rem != 2)
		return 1;

	div_t dn = div(-17, 5);
	if (dn.quot != -3 || dn.rem != -2)
		return 2;

	ldiv_t l = ldiv(1000003L, 1000L);
	if (l.quot != 1000L || l.rem != 3L)
		return 3;

	lldiv_t ll = lldiv(0x100000007LL, 0x100000000LL);
	if (ll.quot != 1 || ll.rem != 7)
		return 4;

	lldiv_t ll2 = lldiv(10000000000LL, 7LL);
	if (ll2.quot != 1428571428LL || ll2.rem != 4LL)
		return 5;

	return 0;
}
