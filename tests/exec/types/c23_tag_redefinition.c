extern int printf(const char *, ...);

typedef struct q {
	int x;
} q_t;
typedef struct q {
	int x;
} q_t;

struct t {
	int a;
	char b : 3;
	long c;
};
struct t {
	int a;
	char b : 3;
	long c;
};

struct self {
	void (*p)(const struct t *);
	int n;
};
struct self {
	void (*p)(const struct t *);
	int n;
};

union u {
	int i;
	double d;
};
union u {
	int i;
	double d;
};

enum e { E0 = 1, E1 = 2 };
enum e { E1 = 2, E0 = 1 };

enum a { b = 7 } y;
enum a { b = _Generic(&y, enum a *: 7, default: 1) };

static void cb(const struct t *s) {
	(void)s;
}

int main(void) {
	int ok = 1;
	q_t q = {5};
	struct t tv = {1, 2, 3};
	union u uv;
	struct self sv;

	sv.p = cb;
	sv.n = 9;
	uv.d = 2.5;

	if (q.x != 5)
		ok = 0;
	if (tv.a != 1 || tv.b != 2 || tv.c != 3)
		ok = 0;
	if (sizeof(struct t) != sizeof(struct q) + sizeof(long) + sizeof(int))
		ok = 0;
	if (uv.d != 2.5)
		ok = 0;
	if (sv.p != cb || sv.n != 9)
		ok = 0;
	if (E0 != 1 || E1 != 2 || b != 7)
		ok = 0;
	if (_Generic(&y, enum a *: 1, default: 0) != 1)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
