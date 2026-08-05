#include <stdint.h>
#include <stdio.h>

__thread int32_t tls_init = 42;
__thread int32_t tls_zero;
static __thread int32_t tls_static = 9;
static __thread int32_t tls_static_zero;

extern __thread int32_t aux_tls_init;
extern int32_t aux_tls_get(void);
extern void aux_tls_set(int32_t v);
extern int32_t aux_tls_zero_get(void);
extern int32_t aux_tls_priv_bump(void);

int main(void) {
	int32_t *p;
	printf("init=%d\n", tls_init);
	printf("zero=%d\n", tls_zero);
	printf("static=%d\n", tls_static);
	printf("staticzero=%d\n", tls_static_zero);
	tls_init = 100;
	tls_zero = 200;
	tls_static = 300;
	tls_static_zero = 400;
	printf("w1=%d\n", tls_init);
	printf("w2=%d\n", tls_zero);
	printf("w3=%d\n", tls_static);
	printf("w4=%d\n", tls_static_zero);
	printf("aux=%d\n", aux_tls_init);
	printf("auxfn=%d\n", aux_tls_get());
	aux_tls_set(55);
	printf("auxset=%d\n", aux_tls_init);
	aux_tls_init = 66;
	printf("auxdirect=%d\n", aux_tls_get());
	printf("auxzero=%d\n", aux_tls_zero_get());
	printf("auxpriv=%d\n", aux_tls_priv_bump());
	p = &tls_init;
	printf("addr=%d\n", *p);
	return 0;
}
