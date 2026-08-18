int main(void) {
	unsigned u = 5;
	int i = -1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
	(void)(u > i);
#pragma GCC diagnostic pop
	return (u > i) ? 0 : 0;
}
