struct P {
	int x, y;
};

static int sum(struct P p) {
	return p.x + p.y;
}

int main(void) {
	struct P a;
	a.x = 40;
	a.y = 2;
	return sum(a);
}
