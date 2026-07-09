struct pt {
	int x, y;
};

int expr_kinds(int a, int b, struct pt *p, int *arr) {
	int paren = (a + b) * 2;
	int neg = -a;
	int deref = *arr;
	int cast = (int)3.5;
	int sz = sizeof a;
	int mem = p->x + arr[b];
	char *s = "literal";
	return paren + neg + deref + cast + sz + mem + (int)(long)s;
}
