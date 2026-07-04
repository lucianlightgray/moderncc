struct s6_7_1_agg {
	int a;
	long b;
	char c;
};
static struct s6_7_1_agg s6_7_1_static_agg;

void s6_7_1_storage_qual_test(void) {

	printf("agg=%d %ld %d\n", s6_7_1_static_agg.a, s6_7_1_static_agg.b,
		   (int)s6_7_1_static_agg.c);

	const const int s6_7_1_cc = 7;
	printf("cc=%d\n", s6_7_1_cc);

	const volatile int s6_7_1_cv = 3;
	volatile const int s6_7_1_vc = 3;
	printf("cvvc=%d\n", s6_7_1_cv == s6_7_1_vc);

	unsigned long long int s6_7_1_ulli = 0xFFFFFFFFFFFFFFFFULL;
	unsigned long long s6_7_1_ull = 0xFFFFFFFFFFFFFFFFULL;
	printf("ull=%d\n", s6_7_1_ulli == s6_7_1_ull);
	long long int s6_7_1_lli = -5;
	signed long long s6_7_1_sll = -5;
	printf("lli=%d\n", s6_7_1_lli == s6_7_1_sll);
	printf("ullsz=%d llsz=%d\n",
		   (int)(sizeof(unsigned long long int) == sizeof(unsigned long long)),
		   (int)(sizeof(signed long long) == sizeof(long long int)));

	signed s6_7_1_s = -1;
	signed int s6_7_1_si = -1;
	int s6_7_1_i = -1;
	printf("intset=%d\n", s6_7_1_s == s6_7_1_si && s6_7_1_si == s6_7_1_i);

	_Atomic(int) s6_7_1_at = 10;
	_Atomic int s6_7_1_aq = 20;
	s6_7_1_at = s6_7_1_at + 5;
	s6_7_1_aq = s6_7_1_aq - 5;
	printf("atomic=%d %d\n", (int)s6_7_1_at, (int)s6_7_1_aq);

	register int s6_7_1_ra[4];
	printf("rasz=%d\n", (int)(sizeof(s6_7_1_ra) == 4 * sizeof(int)));
}
