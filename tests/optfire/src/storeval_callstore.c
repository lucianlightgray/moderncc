extern int printf(const char *, ...);

int sv_dst;
int sv_tmp;
int sv_log[8];
int sv_n;

int sv_sink(int a, int b)
{
	if (sv_n < 8)
		sv_log[sv_n++] = a * 10 + b;
	return a + b * 2;
}

static int sv_leaf(int v)
{
	return v ^ 0x5a;
}

void sv_round(int v)
{
	sv_dst = sv_sink(sv_tmp = v, 2);
	(void)sv_leaf(sv_dst);
	sv_dst = sv_sink(sv_tmp = sv_dst & 7, 5);
}

int main(void)
{
	int i;

	sv_round(3);
	(void)sv_leaf(sv_tmp);
	sv_round(sv_dst & 15);
	for (i = 0; i < sv_n; i++)
		printf("l%d=%d\n", i, sv_log[i]);
	printf("dst=%d tmp=%d n=%d\n", sv_dst, sv_tmp, sv_n);
	return 0;
}
