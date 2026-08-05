#include "aggregates.h"

int32_t t_s1(S1 v) { return v.a; }
int32_t t_s3(S3 v) { return v.a + v.b * 10 + v.c * 100; }
int32_t t_s4(S4 v) { return v.a + v.b * 100; }
int32_t t_s7(S7 v) { return v.a + v.b * 10 + v.c * 1000; }
int32_t t_s8(S8 v) { return v.a - v.b; }
int32_t t_s12(S12 v) { return v.a + v.b * 2 + v.c * 3; }
int32_t t_s16(S16 v) { return (int32_t)(v.a + v.b * 2); }
int32_t t_s24(S24 v) { return (int32_t)(v.a + v.b * 2 + v.c * 3); }
double t_d16(D16 v) { return v.x * 2.0 + v.y; }
double t_f16(F16 v) { return (double)v.x + (double)v.y * 2.0 + (double)v.z * 4.0 + (double)v.w * 8.0; }
double t_d40(D40 v) { return v.a + v.b + v.c + v.d + v.e; }
double t_m12(M12 v) { return (double)v.a + v.b; }
double t_fi8(FI8 v) { return (double)v.x + (double)v.n; }

int32_t t_many(S8 a, S16 b, int32_t c, S12 d, S24 e, int32_t f) {
	return a.a + a.b + (int32_t)(b.a + b.b) + c + d.a + d.b + d.c +
				 (int32_t)(e.a + e.b + e.c) + f;
}

double t_mixfp(D16 a, int32_t b, F16 c, double d, S8 e) {
	return a.x + a.y + (double)b + (double)c.x + (double)c.y + (double)c.z +
				 (double)c.w + d + (double)(e.a + e.b);
}
