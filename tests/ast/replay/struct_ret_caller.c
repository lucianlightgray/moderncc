struct P {
	int x, y;
};

static struct P mk(int a, int b) {
	struct P p;
	p.x = a;
	p.y = b;
	return p;
}

int main(void) {
	struct P r = mk(40, 2);
	return r.x + r.y;
}
