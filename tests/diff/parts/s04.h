void s04_charset_test(void) {
	int s04_a = '\a', s04_b = '\b', s04_f = '\f';
	int s04_n = '\n', s04_r = '\r', s04_t = '\t', s04_v = '\v';
	int s04_vals[7], s04_i, s04_j, s04_distinct = 1;
	char s04_str[] = "AB";
	printf("esc: %d %d %d %d %d %d %d\n",
		   s04_a, s04_b, s04_f, s04_n, s04_r, s04_t, s04_v);
	s04_vals[0] = s04_a;
	s04_vals[1] = s04_b;
	s04_vals[2] = s04_f;
	s04_vals[3] = s04_n;
	s04_vals[4] = s04_r;
	s04_vals[5] = s04_t;
	s04_vals[6] = s04_v;
	for (s04_i = 0; s04_i < 7; s04_i++)
		for (s04_j = s04_i + 1; s04_j < 7; s04_j++)
			if (s04_vals[s04_i] == s04_vals[s04_j])
				s04_distinct = 0;
	printf("esc distinct: %d\n", s04_distinct);
	printf("nul=%d strterm=%d\n", '\0', s04_str[2] == '\0');
	printf("digits=%d %d\n", '9' - '0' == 9, ('0' < '1') && ('1' < '2') && ('8' < '9'));
}

void s04_ident_significance_test(void) {
	int s04_ident_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_first = 11;
	int s04_ident_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_second = 22;
	printf("ident: %d %d\n",
		   s04_ident_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_first,
		   s04_ident_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_second);
}

void s04_limits_test(void) {
	printf("charbit=%d\n", CHAR_BIT >= 8);
	printf("uchar=%d\n", UCHAR_MAX == (unsigned char)-1);
	printf("ushrt=%d\n", USHRT_MAX == (unsigned short)-1);
	printf("uint=%d\n", UINT_MAX == (unsigned int)-1);
	printf("ulong=%d\n", ULONG_MAX == (unsigned long)-1);
	printf("ullong=%d\n", ULLONG_MAX == (unsigned long long)-1);
	printf("schar=%d\n", SCHAR_MAX == UCHAR_MAX / 2 && SCHAR_MIN < 0);
	printf("charrange=%d\n",
		   (CHAR_MIN == SCHAR_MIN && CHAR_MAX == SCHAR_MAX) ||
			   (CHAR_MIN == 0 && CHAR_MAX == UCHAR_MAX));
	printf("intord=%d\n", SCHAR_MAX <= SHRT_MAX && SHRT_MAX <= INT_MAX && INT_MAX <= LONG_MAX && LONG_MAX <= LLONG_MAX);
#if INT_MAX >= 32767 && UINT_MAX >= 65535U && LONG_MAX >= 2147483647L
	printf("ifusable=1\n");
#else
	printf("ifusable=0\n");
#endif
#if ULLONG_MAX >= 18446744073709551615ULL && LLONG_MAX >= 9223372036854775807LL
	printf("llusable=1\n");
#else
	printf("llusable=0\n");
#endif
}

void s04_float_limits_test(void) {
	printf("radix=%d\n", FLT_RADIX >= 2);
	printf("mantorder=%d\n", FLT_MANT_DIG <= DBL_MANT_DIG && DBL_MANT_DIG <= LDBL_MANT_DIG);
	printf("digorder=%d\n", FLT_DIG <= DBL_DIG && DBL_DIG <= LDBL_DIG);
	printf("maxorder=%d\n", (double)FLT_MAX <= DBL_MAX);
	printf("eps=%d\n", FLT_EPSILON > 0.0f && FLT_EPSILON < 1.0f && DBL_EPSILON > 0.0 && DBL_EPSILON < 1.0);
	printf("min=%d\n", FLT_MIN > 0.0f && DBL_MIN > 0.0);
	printf("truemin=%d\n", FLT_TRUE_MIN > 0.0f && FLT_TRUE_MIN <= FLT_MIN);
	printf("decdig=%d\n", DECIMAL_DIG >= 10);
	printf("fltdecdig=%d\n", FLT_DECIMAL_DIG >= 6 && DBL_DECIMAL_DIG >= 10 && LDBL_DECIMAL_DIG >= 10);
	printf("subnorm=%d\n", (FLT_HAS_SUBNORM == 0 || FLT_HAS_SUBNORM == 1 || FLT_HAS_SUBNORM == -1));
	printf("expord=%d\n", FLT_MIN_EXP < 0 && FLT_MAX_EXP > 0 && FLT_MIN_10_EXP <= -37 && FLT_MAX_10_EXP >= 37);
#if FLT_RADIX >= 2 && DBL_MANT_DIG >= 10 && DECIMAL_DIG >= 10
	printf("fltifusable=1\n");
#else
	printf("fltifusable=0\n");
#endif
}
