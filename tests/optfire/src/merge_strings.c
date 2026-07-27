extern int printf(const char *, ...);

static const char *pick(int k)
{
	if (k == 0)
		return "repeated literal payload";
	if (k == 1)
		return "repeated literal payload";
	if (k == 2)
		return "repeated literal payload";
	return "repeated literal payload";
}

int main(void)
{
	int i;
	unsigned long h = 0;
	const char *s;

	for (i = 0; i < 4; i++) {
		s = pick(i);
		while (*s) {
			h = h * 31u + (unsigned long)(unsigned char)*s;
			s++;
		}
	}
	printf("merge_strings=%lu\n", h & 0xffffffUL);
	return 0;
}
