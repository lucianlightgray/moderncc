#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

int32_t v_isum(int32_t n, ...) {
	va_list ap;
	int32_t s = 0, i;
	va_start(ap, n);
	for (i = 0; i < n; i++)
		s += va_arg(ap, int32_t);
	va_end(ap);
	return s;
}

double v_dsum(int32_t n, ...) {
	va_list ap;
	double s = 0.0;
	int32_t i;
	va_start(ap, n);
	for (i = 0; i < n; i++)
		s += va_arg(ap, double);
	va_end(ap);
	return s;
}

double v_mix(int32_t n, ...) {
	va_list ap;
	double s = 0.0;
	int32_t i;
	va_start(ap, n);
	for (i = 0; i < n; i++) {
		s += (double)va_arg(ap, int32_t);
		s += va_arg(ap, double);
	}
	va_end(ap);
	return s;
}

int32_t v_fwd(char *out, int32_t cap, const char *fmt, ...) {
	va_list ap;
	int r;
	va_start(ap, fmt);
	r = vsnprintf(out, (size_t)cap, fmt, ap);
	va_end(ap);
	return (int32_t)r;
}

double v_wide(int32_t n, double d0, ...) {
	va_list ap;
	double s = d0;
	int32_t i;
	va_start(ap, d0);
	for (i = 0; i < n; i++)
		s += va_arg(ap, double);
	va_end(ap);
	return s;
}
