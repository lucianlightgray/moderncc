#include <stdio.h>

struct vec {
	float x;
	float y;
};

void bug(float x, float y) {
	printf("x=%f\ny=%f\n", x, y);
}

float dot(struct vec v) {
	return 999.5;
}

void main(void) {
	struct vec a;
	a.x = 33.0f;
	a.y = 77.0f;
	bug(dot(a), dot(a));
}
