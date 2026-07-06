#include <stdio.h>
#include <stdatomic.h>

int main(void) {
	int arr[16];
	_Atomic(int *) p = arr;

	int *old = atomic_fetch_add(&p, 3);
	printf("%ld %ld\n", (long)(old - arr), (long)((int *)p - arr));

	old = atomic_fetch_sub(&p, 1);
	printf("%ld %ld\n", (long)(old - arr), (long)((int *)p - arr));

	old = atomic_fetch_add_explicit(&p, 4, memory_order_seq_cst);
	printf("%ld %ld\n", (long)(old - arr), (long)((int *)p - arr));

	double darr[8];
	_Atomic(double *) dp = darr;
	double *dold = atomic_fetch_add(&dp, 2);
	printf("%ld %ld\n", (long)(dold - darr), (long)((double *)dp - darr));

	return 0;
}
