#include <stdatomic.h>
extern int printf(const char *, ...);

typedef struct {
	int x, y, z;
} P;
static _Atomic P p;

int main(void) {
	int ok = 1;
	P n = {1, 2, 3};
	atomic_store(&p, n);
	P g = atomic_load(&p);
	if (!(g.x == 1 && g.y == 2 && g.z == 3))
		ok = 0;

	P n2 = {4, 5, 6};
	P old = atomic_exchange(&p, n2);
	if (!(old.x == 1 && old.y == 2 && old.z == 3))
		ok = 0;
	g = atomic_load(&p);
	if (!(g.x == 4 && g.y == 5 && g.z == 6))
		ok = 0;

	P expected = {4, 5, 6}, desired = {7, 8, 9};
	if (!atomic_compare_exchange_strong(&p, &expected, desired))
		ok = 0;
	g = atomic_load(&p);
	if (!(g.x == 7 && g.y == 8 && g.z == 9))
		ok = 0;

	P wrong = {0, 0, 0}, des2 = {1, 1, 1};
	if (atomic_compare_exchange_strong(&p, &wrong, des2))
		ok = 0;
	if (!(wrong.x == 7 && wrong.y == 8 && wrong.z == 9))
		ok = 0;

	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
