struct P {
	int x, y;
};

struct P make(int a, int b) {
	struct P p;
	p.x = a;
	p.y = b;
	return p;
}

int main(void) {
	struct P r = make(40, 2);
	return r.x + r.y;
}
