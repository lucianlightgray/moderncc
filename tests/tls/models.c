#include <stdio.h>

__thread int g_tls = 111;
static __thread int l_tls = 222;
__thread long z_tls;

int get_g(void) {
	return g_tls;
}
int get_l(void) {
	return l_tls;
}

int main(void) {
	g_tls += 1;
	l_tls += 2;
	z_tls += 5;
	printf("g=%d l=%d\n", get_g(), get_l());
	return (get_g() == 112 && get_l() == 224 && z_tls == 5) ? 0 : 1;
}
