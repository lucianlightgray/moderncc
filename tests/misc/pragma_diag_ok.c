int main(void) {
	unsigned u = 5;
	int i = -1;
	int r;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
	r = (u > i);
#pragma GCC diagnostic pop
	return r ? 0 : 0;
}
