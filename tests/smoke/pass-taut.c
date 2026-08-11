extern int printf(const char *, ...);

static volatile int smp_v7 = 7;
static volatile int smp_v0 = 0;
static volatile int smp_vneg = -3;

static int smp_or_taut(int var)
{
	return var <= 0 || var != 0;
}

static int smp_or_taut_neg(int var)
{
	return !(var <= 0 || var != 0);
}

static int smp_or_split(int var)
{
	return var < 5 || var >= 5;
}

static int smp_and_contra(int var)
{
	return var > 10 && var < 3;
}

static int smp_and_contra_neg(int var)
{
	return !(var > 10 && var < 3);
}

static int smp_or_gap(int var)
{
	return var < 2 || var > 4;
}

static int smp_and_live(int var)
{
	return var > 1 && var < 9;
}

int main(void)
{
	int a = smp_v7, b = smp_v0, c = smp_vneg;

	printf("taut %d%d%d %d%d%d %d%d%d %d%d%d %d%d%d %d%d%d %d%d%d\n",
				 smp_or_taut(a), smp_or_taut(b), smp_or_taut(c), smp_or_taut_neg(a),
				 smp_or_taut_neg(b), smp_or_taut_neg(c), smp_or_split(a),
				 smp_or_split(b), smp_or_split(c), smp_and_contra(a),
				 smp_and_contra(b), smp_and_contra(c), smp_and_contra_neg(a),
				 smp_and_contra_neg(b), smp_and_contra_neg(c), smp_or_gap(a),
				 smp_or_gap(b), smp_or_gap(c), smp_and_live(a), smp_and_live(b),
				 smp_and_live(c));
	return 0;
}
