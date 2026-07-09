struct P {
	int x, y;
};

static void copy(struct P *a, struct P *b) {
	*a = *b;
}

int main(void) {
	struct P s, d;
	s.x = 40;
	s.y = 2;
	copy(&d, &s);
	struct P q = d;
	return (*&q).x + q.y;
}
