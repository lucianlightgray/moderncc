#include <stdint.h>
#include <stdio.h>

extern int32_t aux_counter;
extern int32_t aux_add(int32_t a, int32_t b);
extern int32_t aux_mul(int32_t a, int32_t b);
extern int32_t aux_via_static(int32_t x);
extern int32_t aux_read_main(void);
extern int32_t aux_call_back(int32_t x);

int32_t main_shared = 41;

int32_t main_callback(int32_t x) { return x * 10; }

int main(void) {
	printf("add=%d\n", aux_add(20, 22));
	printf("mul=%d\n", aux_mul(6, 7));
	printf("static=%d\n", aux_via_static(5));
	printf("counter=%d\n", aux_counter);
	aux_counter += 5;
	printf("counter2=%d\n", aux_counter);
	printf("shared=%d\n", aux_read_main());
	main_shared = 99;
	printf("shared2=%d\n", aux_read_main());
	printf("callback=%d\n", aux_call_back(4));
	return 0;
}
