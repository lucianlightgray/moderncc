#include "aggregates.h"

S1 r_s1(int32_t seed) { S1 v; v.a = (int8_t)seed; return v; }

S3 r_s3(int32_t seed) {
	S3 v;
	v.a = (int8_t)seed;
	v.b = (int8_t)(seed + 1);
	v.c = (int8_t)(seed + 2);
	return v;
}

S4 r_s4(int32_t seed) {
	S4 v;
	v.a = (int16_t)seed;
	v.b = (int16_t)(seed * 2);
	return v;
}

S7 r_s7(int32_t seed) {
	S7 v;
	v.a = seed;
	v.b = (int16_t)(seed + 1);
	v.c = (int8_t)(seed + 2);
	return v;
}

S8 r_s8(int32_t seed) {
	S8 v;
	v.a = seed;
	v.b = seed * 3;
	return v;
}

S12 r_s12(int32_t seed) {
	S12 v;
	v.a = seed;
	v.b = seed + 1;
	v.c = seed + 2;
	return v;
}

S16 r_s16(int32_t seed) {
	S16 v;
	v.a = seed;
	v.b = (int64_t)seed * 1000;
	return v;
}

S24 r_s24(int32_t seed) {
	S24 v;
	v.a = seed;
	v.b = seed * 2;
	v.c = seed * 3;
	return v;
}

D16 r_d16(double seed) {
	D16 v;
	v.x = seed;
	v.y = seed * 2.0;
	return v;
}

F16 r_f16(float seed) {
	F16 v;
	v.x = seed;
	v.y = seed * 2.0f;
	v.z = seed * 4.0f;
	v.w = seed * 8.0f;
	return v;
}

D40 r_d40(double seed) {
	D40 v;
	v.a = seed;
	v.b = seed + 1.0;
	v.c = seed + 2.0;
	v.d = seed + 3.0;
	v.e = seed + 4.0;
	return v;
}

M12 r_m12(int32_t seed) {
	M12 v;
	v.a = seed;
	v.b = (double)seed / 4.0;
	return v;
}

FI8 r_fi8(int32_t seed) {
	FI8 v;
	v.x = (float)seed / 2.0f;
	v.n = seed;
	return v;
}
