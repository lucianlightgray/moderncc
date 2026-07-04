extern int printf(const char *, ...);

int main(void) {
	int x = 5;
	long lx = 100;

	__atomic_store_n(&x, 10, __ATOMIC_SEQ_CST);
	int l = __atomic_load_n(&x, __ATOMIC_SEQ_CST);
	int e = __atomic_exchange_n(&x, 20, __ATOMIC_SEQ_CST);

	int exp = 20, des = 30;
	int ok = __atomic_compare_exchange_n(&x, &exp, des, 0,
										 __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	int bad = __atomic_compare_exchange_n(&x, &exp, 99, 0,
										  __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

	__atomic_store_n(&lx, 7L, __ATOMIC_RELAXED);
	long ll = __atomic_load_n(&lx, __ATOMIC_RELAXED);

	int pass = l == 10 && e == 10 && x == 30 && ok == 1 && exp == 30 && bad == 0 && ll == 7;
	printf(pass ? "OK\n" : "FAIL\n");
	return pass ? 0 : 1;
}
