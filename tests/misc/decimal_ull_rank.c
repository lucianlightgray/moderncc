int main(void) {
	if (_Generic(10000000000000000000,
				unsigned long long: 1, default: 0) != 1) return 1;
	if (10000000000000000000 != 10000000000000000000ULL) return 2;
	return 0;
}
