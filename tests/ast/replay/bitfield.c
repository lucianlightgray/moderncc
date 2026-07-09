struct F {
	unsigned a : 3, b : 5, c : 8;
	int s : 10;
};

int main(void) {
	struct F f;
	f.a = 5;
	f.b = 20;
	f.c = 100;
	f.s = -83;
	return f.a + f.b + f.c + f.s;
}
