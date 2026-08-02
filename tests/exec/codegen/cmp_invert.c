extern int printf(const char *, ...);
























static volatile int sink;

static int mat(int x)
{
	sink = x;
	return sink;
}

enum { C_EQ, C_NE, C_LT, C_LE, C_GT, C_GE, C_ULT, C_ULE, C_UGT, C_UGE, C_NN_LT,
			 C_LLT, C_LGE, C_ULLT, C_ULGE, C_COUNT };


static int inv(int which, long long a, long long b)
{
	unsigned long long ua = (unsigned long long)a, ub = (unsigned long long)b;
	int ia = (int)a, ib = (int)b;
	unsigned uia = (unsigned)ia, uib = (unsigned)ib;

	switch (which) {
	case C_EQ: return !(ia == ib);
	case C_NE: return !(ia != ib);
	case C_LT: return !(ia < ib);
	case C_LE: return !(ia <= ib);
	case C_GT: return !(ia > ib);
	case C_GE: return !(ia >= ib);
	case C_ULT: return !(uia < uib);
	case C_ULE: return !(uia <= uib);
	case C_UGT: return !(uia > uib);
	case C_UGE: return !(uia >= uib);
	case C_NN_LT: return !!(ia < ib);
	case C_LLT: return !(a < b);
	case C_LGE: return !(a >= b);
	case C_ULLT: return !(ua < ub);
	case C_ULGE: return !(ua >= ub);
	default: return -1;
	}
}


static int ref(int which, long long a, long long b)
{
	unsigned long long ua = (unsigned long long)a, ub = (unsigned long long)b;
	int ia = (int)a, ib = (int)b;
	unsigned uia = (unsigned)ia, uib = (unsigned)ib;

	switch (which) {
	case C_EQ: return 1 - (ia == ib);
	case C_NE: return 1 - (ia != ib);
	case C_LT: return 1 - (ia < ib);
	case C_LE: return 1 - (ia <= ib);
	case C_GT: return 1 - (ia > ib);
	case C_GE: return 1 - (ia >= ib);
	case C_ULT: return 1 - (uia < uib);
	case C_ULE: return 1 - (uia <= uib);
	case C_UGT: return 1 - (uia > uib);
	case C_UGE: return 1 - (uia >= uib);
	case C_NN_LT: return (ia < ib);
	case C_LLT: return 1 - (a < b);
	case C_LGE: return 1 - (a >= b);
	case C_ULLT: return 1 - (ua < ub);
	case C_ULGE: return 1 - (ua >= ub);
	default: return -1;
	}
}

int main(void)
{
	static const long long w[] = {-9223372036854775807LL - 1, -2147483647 - 1LL,
																-7, -1, 0, 1, 7, 2147483647LL,
																9223372036854775807LL};
	int i, j, k, bad = 0;
	unsigned long acc = 0;

	for (i = 0; i < (int)(sizeof w / sizeof w[0]); i++) {
		for (j = 0; j < (int)(sizeof w / sizeof w[0]); j++) {
			for (k = 0; k < C_COUNT; k++) {
				int got = mat(inv(k, w[i], w[j]));
				int want = mat(ref(k, w[i], w[j]));
				if (got != want) {
					if (!bad)
						printf("FAIL op=%d a=%lld b=%lld got=%d want=%d\n", k, w[i], w[j],
									 got, want);
					bad = 1;
				}
				acc = acc * 31 + (unsigned long)got;
			}
		}
	}

	printf("acc=%lu\n", acc);
	printf(bad ? "FAIL\n" : "OK\n");
	return bad;
}
