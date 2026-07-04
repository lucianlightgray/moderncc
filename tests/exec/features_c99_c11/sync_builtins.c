extern int printf(const char *, ...);

int main(void) {
	int x = 10;
	int a = __sync_fetch_and_add(&x, 5);
	int b = __sync_add_and_fetch(&x, 5);
	int c = __sync_fetch_and_or(&x, 1);
	int ok1 = __sync_bool_compare_and_swap(&x, 21, 100);
	int v = __sync_val_compare_and_swap(&x, 100, 7);
	int lock = 0;
	int prev = __sync_lock_test_and_set(&lock, 1);
	int held = lock;
	__sync_lock_release(&lock);
	__sync_synchronize();
	int y = ++x;
	__sync_synchronize();
	int ok = a == 10 && b == 20 && c == 20 && ok1 == 1 && v == 100 && x == 8 && prev == 0 && held == 1 && lock == 0 && y == 8;
	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
