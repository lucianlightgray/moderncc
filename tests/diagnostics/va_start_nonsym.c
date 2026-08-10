#include <stdarg.h>

int va_start_nonsym_cmp(int x, ...)
{
	va_list ap;
	int n;
	va_start(ap, 1 < x);
	n = va_arg(ap, int);
	va_end(ap);
	return n;
}

int va_start_nonsym_cmp_rev(int x, ...)
{
	va_list ap;
	int n;
	va_start(ap, x < 1);
	n = va_arg(ap, int);
	va_end(ap);
	return n;
}

int va_start_nonsym_expr(int x, ...)
{
	va_list ap;
	int n;
	va_start(ap, x + 1);
	n = va_arg(ap, int);
	va_end(ap);
	return n;
}

int va_start_named(int x, ...)
{
	va_list ap;
	int n;
	va_start(ap, x);
	n = va_arg(ap, int);
	va_end(ap);
	return n;
}
