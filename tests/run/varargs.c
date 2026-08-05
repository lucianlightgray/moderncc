#include <stdint.h>
#include <stdio.h>

extern int32_t v_isum(int32_t n, ...);
extern double v_dsum(int32_t n, ...);
extern double v_mix(int32_t n, ...);
extern int32_t v_fwd(char *out, int32_t cap, const char *fmt, ...);
extern double v_wide(int32_t n, double d0, ...);

int main(void) {
	char buf[128];
	printf("isum=%d\n", v_isum(6, 1, 2, 3, 4, 5, 6));
	printf("isum12=%d\n",
				 v_isum(12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12));
	printf("dsum=%.6f\n", v_dsum(6, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5));
	printf("dsum10=%.6f\n",
				 v_dsum(10, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0));
	printf("mix=%.6f\n", v_mix(4, 1, 0.25, 2, 0.5, 3, 0.75, 4, 1.5));
	printf("wide=%.6f\n", v_wide(5, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5));
	printf("fwd=%d\n", v_fwd(buf, (int32_t)sizeof buf, "%d/%s/%.2f", 7, "x", 1.5));
	printf("fwdbuf=%s\n", buf);
	return 0;
}
