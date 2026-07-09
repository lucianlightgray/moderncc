#define INNER 5
#define OUTER(x) ((x) + 1)

int g(void) {
	return OUTER(INNER);
}
