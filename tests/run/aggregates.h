#ifndef MCC_TESTS_RUN_AGGREGATES_H
#define MCC_TESTS_RUN_AGGREGATES_H

#include <stdint.h>

typedef struct { int8_t a; } S1;
typedef struct { int8_t a, b, c; } S3;
typedef struct { int16_t a, b; } S4;
typedef struct { int32_t a; int16_t b; int8_t c; } S7;
typedef struct { int32_t a, b; } S8;
typedef struct { int32_t a, b, c; } S12;
typedef struct { int64_t a, b; } S16;
typedef struct { int64_t a, b, c; } S24;
typedef struct { double x, y; } D16;
typedef struct { float x, y, z, w; } F16;
typedef struct { double a, b, c, d, e; } D40;
typedef struct { int32_t a; double b; } M12;
typedef struct { float x; int32_t n; } FI8;

int32_t t_s1(S1 v);
int32_t t_s3(S3 v);
int32_t t_s4(S4 v);
int32_t t_s7(S7 v);
int32_t t_s8(S8 v);
int32_t t_s12(S12 v);
int32_t t_s16(S16 v);
int32_t t_s24(S24 v);
double t_d16(D16 v);
double t_f16(F16 v);
double t_d40(D40 v);
double t_m12(M12 v);
double t_fi8(FI8 v);
int32_t t_many(S8 a, S16 b, int32_t c, S12 d, S24 e, int32_t f);
double t_mixfp(D16 a, int32_t b, F16 c, double d, S8 e);

S1 r_s1(int32_t seed);
S3 r_s3(int32_t seed);
S4 r_s4(int32_t seed);
S7 r_s7(int32_t seed);
S8 r_s8(int32_t seed);
S12 r_s12(int32_t seed);
S16 r_s16(int32_t seed);
S24 r_s24(int32_t seed);
D16 r_d16(double seed);
F16 r_f16(float seed);
D40 r_d40(double seed);
M12 r_m12(int32_t seed);
FI8 r_fi8(int32_t seed);

#endif
