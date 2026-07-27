extern int printf(const char *, ...);

struct node {
	int val;
	int step;
	struct node *next;
};

static int walk(struct node *head, int rounds)
{
	struct node *p;
	int total = 0;
	int i;

	for (i = 0; i < rounds; i++) {
		p = head;
		while (p != 0) {
			total += p->val;
			total ^= p->step;
			p->val = (p->val + p->step) & 0xffff;
			p = p->next;
		}
	}
	return total & 0xffff;
}

int main(void)
{
	struct node c, b, a;

	c.val = 7; c.step = 3; c.next = 0;
	b.val = 5; b.step = 2; b.next = &c;
	a.val = 1; a.step = 9; a.next = &b;
	printf("promo_arrow=%d\n", walk(&a, 4));
	return 0;
}
