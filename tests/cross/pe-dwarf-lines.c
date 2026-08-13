int square(int x) {
	int r = x * x;
	return r;
}

int cube(int x) {
	int r = square(x) * x;
	return r;
}

int main(void) {
	int a = square(3);
	int b = cube(2);
	return a + b - 17;
}
