#include <stdarg.h>

struct P {
	int x, y;
};

static struct P mkv(int n, ...) {
	va_list ap;
	va_start(ap, n);
	struct P p;
	p.x = va_arg(ap, int);
	p.y = n;
	va_end(ap);
	return p;
}

int use(void) {
	struct P p = mkv(5, 37);
	return p.x + p.y;
}

int main(void) {
	return use();
}
