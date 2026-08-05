#include <stdio.h>

#define A(x, ...) x##,##__VA_ARGS__
#define B(b, ...) {b, ##__VA_ARGS__}
#define C(...) 10 , ## __VA_ARGS__ + 5

int main(void) {
	int v[] = B(1, 2, 3);
	int w[] = B(4);
	if (A(7) == 7 && v[0] == 1 && v[2] == 3 && w[0] == 4 && C() == 15)
		printf("OK\n");
	return 0;
}
