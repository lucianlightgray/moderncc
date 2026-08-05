#include "aggregates.h"

#include <stdio.h>

int main(void) {
	S1 s1 = r_s1(11);
	S3 s3 = r_s3(1);
	S4 s4 = r_s4(21);
	S7 s7 = r_s7(31);
	S8 s8 = r_s8(41);
	S12 s12 = r_s12(51);
	S16 s16 = r_s16(61);
	S24 s24 = r_s24(71);
	D16 d16 = r_d16(1.5);
	F16 f16 = r_f16(0.5f);
	D40 d40 = r_d40(10.0);
	M12 m12 = r_m12(9);
	FI8 fi8 = r_fi8(7);

	printf("s1=%d\n", (int)s1.a);
	printf("s3=%d,%d,%d\n", (int)s3.a, (int)s3.b, (int)s3.c);
	printf("s4=%d,%d\n", (int)s4.a, (int)s4.b);
	printf("s7=%d,%d,%d\n", (int)s7.a, (int)s7.b, (int)s7.c);
	printf("s8=%d,%d\n", (int)s8.a, (int)s8.b);
	printf("s12=%d,%d,%d\n", (int)s12.a, (int)s12.b, (int)s12.c);
	printf("s16=%d,%d\n", (int)s16.a, (int)s16.b);
	printf("s24=%d,%d,%d\n", (int)s24.a, (int)s24.b, (int)s24.c);
	printf("d16=%.6f,%.6f\n", d16.x, d16.y);
	printf("f16=%.6f,%.6f,%.6f,%.6f\n", (double)f16.x, (double)f16.y,
				 (double)f16.z, (double)f16.w);
	printf("d40=%.6f,%.6f,%.6f,%.6f,%.6f\n", d40.a, d40.b, d40.c, d40.d, d40.e);
	printf("m12=%d,%.6f\n", (int)m12.a, m12.b);
	printf("fi8=%.6f,%d\n", (double)fi8.x, (int)fi8.n);
	printf("chain=%d\n", (int)r_s8(r_s12(3).c).b);
	printf("field=%.6f\n", r_d40(0.5).e);
	return 0;
}
