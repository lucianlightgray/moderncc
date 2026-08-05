#include "aggregates.h"

#include <stdio.h>

int main(void) {
	S1 s1 = {5};
	S3 s3 = {1, 2, 3};
	S4 s4 = {7, 9};
	S7 s7 = {4, 5, 6};
	S8 s8 = {30, 12};
	S12 s12 = {1, 2, 3};
	S16 s16 = {100, 200};
	S24 s24 = {1, 2, 3};
	D16 d16 = {1.5, 2.25};
	F16 f16 = {0.5f, 1.5f, 2.5f, 3.5f};
	D40 d40 = {1.0, 2.0, 4.0, 8.0, 16.0};
	M12 m12 = {7, 0.5};
	FI8 fi8 = {1.25f, 9};

	printf("s1=%d\n", t_s1(s1));
	printf("s3=%d\n", t_s3(s3));
	printf("s4=%d\n", t_s4(s4));
	printf("s7=%d\n", t_s7(s7));
	printf("s8=%d\n", t_s8(s8));
	printf("s12=%d\n", t_s12(s12));
	printf("s16=%d\n", t_s16(s16));
	printf("s24=%d\n", t_s24(s24));
	printf("d16=%.6f\n", t_d16(d16));
	printf("f16=%.6f\n", t_f16(f16));
	printf("d40=%.6f\n", t_d40(d40));
	printf("m12=%.6f\n", t_m12(m12));
	printf("fi8=%.6f\n", t_fi8(fi8));
	printf("many=%d\n", t_many(s8, s16, 1000, s12, s24, 2000));
	printf("mixfp=%.6f\n", t_mixfp(d16, 3, f16, 0.25, s8));
	return 0;
}
