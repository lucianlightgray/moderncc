struct P {
	int x, y;
};

static int sum(struct P *p) {
	return p->x + p->y;
}

int main(void) {
	struct P p;
	p.x = 30;
	p.y = 12;
	p.x += p.y;
	return sum(&p) - p.y;
}
