#include <stdio.h>

union vx { short f[8]; int v; };
union U { int i; float f; char *p; };
struct S { int a, b; };
union W { struct S s; long long l; };

char msg[] = "hi";

static union U su = (union U) 7;
static union U sp = (union U) msg;

int sink;

void foo5(int vec)
{
	union vx u = (union vx) vec;
	sink = u.v;
}

union U mk(float x) { return (union U) x; }

int take(union U u) { return u.i; }

int main(void)
{
	int a = 42;
	float fv = 2.5f;
	char buf[4];
	union U u;
	union vx w;
	union W ws;
	struct S s;
	int ok = 1;

	s.a = 5;
	s.b = 6;
	ws = (union W) s;
	if (ws.s.a != 5 || ws.s.b != 6)
		ok = 0;
	if (sp.p != msg)
		ok = 0;
	if (((union U) msg).p != msg)
		ok = 0;

	if (((union U) a).i != 42)
		ok = 0;
	if (((union U) fv).f != 2.5f)
		ok = 0;
	if (((union U) (char *) buf).p != buf)
		ok = 0;
	if (su.i != 7)
		ok = 0;
	if (mk(1.25f).f != 1.25f)
		ok = 0;
	if (take((union U) 13) != 13)
		ok = 0;
	u = (union U) 9;
	if (u.i != 9)
		ok = 0;
	w = (union vx) 0;
	((union vx) a).f[5] = 1;
	if (w.v != 0)
		ok = 0;
	foo5(11);
	if (sink != 11)
		ok = 0;
	if (sizeof((union U) a) != sizeof(union U))
		ok = 0;
	printf(ok ? "OK\n" : "FAIL\n");
	return 0;
}
