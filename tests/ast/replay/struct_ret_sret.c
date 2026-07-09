struct Big {
	int a, b, c, d, e;
};

static struct Big mk(int v) {
	struct Big b;
	b.a = v;
	b.b = v;
	b.c = v;
	b.d = v;
	b.e = v;
	return b;
}

int main(void) {
	return mk(42).a;
}
