int s6_7_6_counter = 0;
int s6_7_6_getn(void) {
	s6_7_6_counter++;
	return 4;
}

int s6_7_6_cbfn(void) {
	return 77;
}
int s6_7_6_mdfunc(void) {
	return 55;
}

void s6_7_6_apply(int cb(void)) {
	printf("apply=%d\n", cb());
}

void s6_7_6_arrparam(int a[10]) {
	printf("arrparam_sizeof_is_ptr=%d\n", (int)(sizeof a == sizeof(int *)));
}

void s6_7_6_declarators(void) {

	int *p, arr[3], (*fp)(void);
	arr[0] = 10;
	arr[1] = 20;
	arr[2] = 30;
	p = arr;
	fp = s6_7_6_mdfunc;
	printf("multi: %d %d %d\n",
				 (int)(sizeof arr == 3 * sizeof(int)),
				 p[0] + p[2],
				 fp());

	s6_7_6_apply(s6_7_6_cbfn);

	{
		int local[10];
		s6_7_6_arrparam(local);
	}

	s6_7_6_counter = 0;
	{
		int v[s6_7_6_getn()];
		int firstsz = sizeof v;
		int secondsz = sizeof v;
		printf("vla_once: %d %d %d\n",
					 s6_7_6_counter,
					 (int)(firstsz == 4 * (int)sizeof(int)),
					 (int)(firstsz == secondsz));
	}

	{
		int n = 3;
		int w[n];
		n = 100;
		printf("vla_fixed: %d\n", (int)(sizeof w == 3 * sizeof(int)));
	}

	s6_7_6_counter = 0;
	{
		int a = s6_7_6_getn();
		int b = s6_7_6_getn();
		int m[a][b];
		printf("vla_order: %d %d\n",
					 s6_7_6_counter,
					 (int)(sizeof m == (size_t)a * b * sizeof(int)));
	}
}
