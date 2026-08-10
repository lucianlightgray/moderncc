extern int printf(const char *, ...);

struct smp_tag {
	char a;
};

static int smp_if_tag(void)
{
	if (sizeof(struct smp_tag { int a; double b; char *c; void *d; }))
		(void)0;
	return (int)sizeof(struct smp_tag);
}

static int smp_while_tag(void)
{
	while (0 && sizeof(struct smp_wtag { double a; double b; }))
		(void)0;
	return (int)sizeof(struct smp_wtag);
}

static int smp_switch_tag(void)
{
	switch (sizeof(struct smp_stag { double a; double b; double c; })) {
	default:
		break;
	}
	return (int)sizeof(struct smp_stag);
}

int main(void)
{
	printf("c90tag %d %d %d %d\n", smp_if_tag(), smp_while_tag(),
				 smp_switch_tag(), (int)sizeof(struct smp_tag));
	return 0;
}
