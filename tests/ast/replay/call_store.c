static int add1(int x) {
	return x + 1;
}

int main(void) {
	int a = add1(41);
	int b;
	b = add1(a);
	return b - 1;
}
